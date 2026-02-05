/*
 * txt2tex.c - Text to LaTeX Converter
 *
 * This program converts a text file containing formatted messages exported from Signal
 * into a LaTeX document suitable for compilation with lualatex.
 *
 * Usage:
 *   txt2tex <input_file>
 *
 * The program reads the specified input text file and generates an output file
 * with the same name but with a .tex extension. For example, if the input file
 * is "messages.txt", the output will be "messages.tex".
 *
 * Operation:
 *   - Reads the input text file line by line
 *   - Processes attachment references and matches them with files in the
 *     "./attachments" directory by filename or file size
 *   - Escapes special LaTeX characters in text content
 *   - Handles UTF-8 characters and emojis using appropriate LaTeX commands
 *   - Filters out unwanted metadata lines (Type:, Received:, etc.)
 *   - Strips phone numbers from "From:" lines
 *   - Generates a complete LaTeX document with proper preamble and formatting
 *
 * Attachment Processing:
 *   - Scans the "./attachments" directory for available files
 *   - Matches attachments by exact filename first, then by file size
 *   - Images are included using \includegraphics
 *   - Non-image attachments are listed as text references
 *   - Unmatched attachments are noted in the output
 *
 * Output Format:
 *   - Creates a LaTeX article document with A4 paper size
 *   - Uses fontspec for Unicode support (requires lualatex)
 *   - Configures emoji font support (Segoe UI Emoji on Windows)
 *   - Preserves line breaks in the original text
 *
 * Compile with:
 *   gcc -o txt2tex txt2tex.c
 *
 * Run with:
 *   ./txt2tex <input_file>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#define MaxPathLen 4096

typedef struct
{
   char fileName[MaxPathLen];
   char fullPath[MaxPathLen];
   long long fileSize;
   int used;
} AttachmentFile;

typedef struct
{
   AttachmentFile *items;
   size_t count;
   size_t capacity;
} AttachmentList;

static void fatal(const char *msg)
{
   fprintf(stderr, "Error: %s\n", msg);
   exit(1);
}

static int hasImageExtension(const char *name)
{
   const char *dot = strrchr(name, '.');
   if(!dot || dot == name)
   {
      return 0;
   }
   dot++;

   char ext[16];
   size_t n = strlen(dot);
   if(n >= sizeof(ext))
   {
      return 0;
   }
   for(size_t i = 0; i < n; i++)
   {
      ext[i] = (char)tolower((unsigned char)dot[i]);
   }
   ext[n] = '\0';

   if(strcmp(ext, "png") == 0) return 1;
   if(strcmp(ext, "jpg") == 0) return 1;
   if(strcmp(ext, "jpeg") == 0) return 1;
   if(strcmp(ext, "gif") == 0) return 1;
   if(strcmp(ext, "bmp") == 0) return 1;
   if(strcmp(ext, "tif") == 0) return 1;
   if(strcmp(ext, "tiff") == 0) return 1;

   return 0;
}

static void attachmentListInit(AttachmentList *list)
{
   list->items = NULL;
   list->count = 0;
   list->capacity = 0;
}

static void attachmentListPush(AttachmentList *list, const AttachmentFile *item)
{
   if(list->count >= list->capacity)
   {
      size_t newCap = (list->capacity == 0) ? 64 : (list->capacity * 2);
      AttachmentFile *newItems = (AttachmentFile *)realloc(list->items, newCap * sizeof(AttachmentFile));
      if(!newItems)
      {
         fatal("Out of memory reallocating attachment list");
      }
      list->items = newItems;
      list->capacity = newCap;
   }
   list->items[list->count++] = *item;
}

static void attachmentListFree(AttachmentList *list)
{
   free(list->items);
   list->items = NULL;
   list->count = 0;
   list->capacity = 0;
}

static void loadAttachmentsDir(const char *dirPath, AttachmentList *list)
{
   DIR *dir = opendir(dirPath);
   if(!dir)
   {
      fprintf(stderr, "Error: could not open attachments directory '%s': %s\n", dirPath, strerror(errno));
      exit(1);
   }

   struct dirent *ent;
   while((ent = readdir(dir)) != NULL)
   {
      if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      {
         continue;
      }

      char fullPath[MaxPathLen];
      snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, ent->d_name);

      struct stat st;
      if(stat(fullPath, &st) != 0)
      {
         continue;
      }
      if(!S_ISREG(st.st_mode))
      {
         continue;
      }

      AttachmentFile f;
      memset(&f, 0, sizeof(f));
      snprintf(f.fileName, sizeof(f.fileName), "%s", ent->d_name);
      snprintf(f.fullPath, sizeof(f.fullPath), "%s", fullPath);
      f.fileSize = (long long)st.st_size;
      f.used = 0;

      attachmentListPush(list, &f);
   }

   closedir(dir);
}

static int utf8CharLen(unsigned char c)
{
   if((c & 0x80) == 0) return 1;
   if((c & 0xE0) == 0xC0) return 2;
   if((c & 0xF0) == 0xE0) return 3;
   if((c & 0xF8) == 0xF0) return 4;
   return 1;
}

static void writeLatexEscaped(FILE *out, const char *s)
{
   const unsigned char *p = (const unsigned char *)s;

   while(*p)
   {
      if(*p < 0x80)
      {
         switch(*p)
         {
            case '\\': fputs("\\textbackslash{}", out); break;
            case '{':  fputs("\\{", out); break;
            case '}':  fputs("\\}", out); break;
            case '#':  fputs("\\#", out); break;
            case '$':  fputs("\\$", out); break;
            case '%':  fputs("\\%", out); break;
            case '&':  fputs("\\&", out); break;
            case '_':  fputs("\\_", out); break;
            case '^':  fputs("\\textasciicircum{}", out); break;
            case '~':  fputs("\\textasciitilde{}", out); break;
            default:
               fputc(*p, out);
               break;
         }
         p++;
      }
      else
      {
         int len = utf8CharLen(*p);

         fputs("\\emoji{", out);
         fwrite(p, 1, len, out);
         fputs("}", out);

         p += len;
      }
   }
}

static void trimRight(char *s)
{
   size_t n = strlen(s);
   while(n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || isspace((unsigned char)s[n - 1])))
   {
      s[n - 1] = '\0';
      n--;
   }
}

static int startsWith(const char *s, const char *prefix)
{
   return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void parseAttachmentLine(const char *line, char *outName, size_t outNameCap, char *outMime, size_t outMimeCap, long long *outBytes, int *outHasName)
{
   // Expected patterns:
   //   Attachment: no filename (image/jpeg, 439593 bytes)
   //   Attachment: myImage.png (image/png, 311164 bytes)

   *outHasName = 0;
   *outBytes = -1;
   outName[0] = '\0';
   outMime[0] = '\0';

   const char *p = line;
   if(!startsWith(p, "Attachment:"))
   {
      return;
   }
   p += strlen("Attachment:");
   while(*p && isspace((unsigned char)*p))
   {
      p++;
   }

   const char *paren = strchr(p, '(');
   if(!paren)
   {
      return;
   }

   char namePart[MaxPathLen];
   size_t nameLen = (size_t)(paren - p);
   if(nameLen >= sizeof(namePart))
   {
      nameLen = sizeof(namePart) - 1;
   }
   memcpy(namePart, p, nameLen);
   namePart[nameLen] = '\0';
   trimRight(namePart);

   if(strcmp(namePart, "no filename") != 0 && namePart[0] != '\0')
   {
      *outHasName = 1;
      snprintf(outName, outNameCap, "%s", namePart);
   }

   // Inside parentheses: "<mime>, <bytes> bytes"
   const char *inside = paren + 1;
   const char *endParen = strchr(inside, ')');
   if(!endParen)
   {
      return;
   }

   char inner[MaxPathLen];
   size_t innerLen = (size_t)(endParen - inside);
   if(innerLen >= sizeof(inner))
   {
      innerLen = sizeof(inner) - 1;
   }
   memcpy(inner, inside, innerLen);
   inner[innerLen] = '\0';
   trimRight(inner);

   // Split on comma
   char *comma = strchr(inner, ',');
   if(!comma)
   {
      return;
   }
   *comma = '\0';
   char *mime = inner;
   char *rest = comma + 1;

   while(*rest && isspace((unsigned char)*rest))
   {
      rest++;
   }

   trimRight(mime);

   snprintf(outMime, outMimeCap, "%s", mime);

   // Parse "<number> bytes"
   // rest could be: "439593 bytes"
   long long bytesVal = -1;
   if(sscanf(rest, "%lld", &bytesVal) == 1)
   {
      *outBytes = bytesVal;
   }
}

static int isImageMime(const char *mime)
{
   return (mime && startsWith(mime, "image/"));
}

static int findAttachmentByExactName(AttachmentList *list, const char *name)
{
   for(size_t i = 0; i < list->count; i++)
   {
      if(list->items[i].used)
      {
         continue;
      }
      if(strcmp(list->items[i].fileName, name) == 0)
      {
         return (int)i;
      }
   }
   return -1;
}

static int findAttachmentBySize(AttachmentList *list, long long size, int preferImage)
{
   int bestIdx = -1;

   // First pass: exact size and preferred image-ness if requested
   if(preferImage)
   {
      for(size_t i = 0; i < list->count; i++)
      {
         if(list->items[i].used)
         {
            continue;
         }
         if(list->items[i].fileSize == size && hasImageExtension(list->items[i].fileName))
         {
            return (int)i;
         }
      }
   }

   // Second pass: any file with matching size
   for(size_t i = 0; i < list->count; i++)
   {
      if(list->items[i].used)
      {
         continue;
      }
      if(list->items[i].fileSize == size)
      {
         bestIdx = (int)i;
         break;
      }
   }

   return bestIdx;
}

static void writeImageInclude(FILE *out, const char *relPath)
{
   fputs("\n\\par\\noindent\n", out);
   fputs("\\includegraphics[width=\\linewidth,height=0.9\\textheight,keepaspectratio]{\\detokenize{", out);
   fputs(relPath, out);
   fputs("}}\n", out);
   fputs("\\par\\medskip\n\n", out);
}

static void writeNonImageAttachment(FILE *out, const char *relPath)
{
   fputs("\n\\begin{quote}\n", out);
   fputs("\\textbf{Attachment:} \\detokenize{", out);
   fputs(relPath, out);
   fputs("}\n", out);
   fputs("\\end{quote}\n\n", out);
}

static int startsWithIgnoreCase(const char *s, const char *prefix)
{
   while(*prefix)
   {
      if(tolower((unsigned char)*s) != tolower((unsigned char)*prefix))
      {
         return 0;
      }
      s++;
      prefix++;
   }
   return 1;
}

static void stripPhoneFromFromLine(char *line)
{
   // Line format: "From: Name (extra stuff)"
   char *colon = strchr(line, ':');
   if(!colon)
   {
      return;
   }

   char *openParen = strchr(colon, '(');
   if(!openParen)
   {
      return;
   }

   *openParen = '\0';  // Truncate at start of parentheses
   trimRight(line);
}

int main(int argc, char *argv[])
{
   if(argc < 2)
   {
      fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
      return 1;
   }

   const char *inputPath = argv[1];

   // Generate output filename by replacing extension with .tex
   char outputPath[MaxPathLen];
   const char *lastDot = strrchr(inputPath, '.');
   if(lastDot && lastDot != inputPath)
   {
      size_t baseLen = (size_t)(lastDot - inputPath);
      if(baseLen >= sizeof(outputPath))
      {
         baseLen = sizeof(outputPath) - 5;  // Leave room for ".tex\0"
      }
      memcpy(outputPath, inputPath, baseLen);
      outputPath[baseLen] = '\0';
      strcat(outputPath, ".tex");
   }
   else
   {
      snprintf(outputPath, sizeof(outputPath), "%s.tex", inputPath);
   }

   const char *attachmentsDir = "./attachments";

   AttachmentList list;
   attachmentListInit(&list);
   loadAttachmentsDir(attachmentsDir, &list);

   FILE *in = fopen(inputPath, "rb");
   if(!in)
   {
      fprintf(stderr, "Error: could not open '%s': %s\n", inputPath, strerror(errno));
      attachmentListFree(&list);
      return 1;
   }

   FILE *out = fopen(outputPath, "wb");
   if(!out)
   {
      fprintf(stderr, "Error: could not open '%s' for writing: %s\n", outputPath, strerror(errno));
      fclose(in);
      attachmentListFree(&list);
      return 1;
   }

   // Minimal LaTeX wrapper
   fputs("\\documentclass[a4paper,11pt]{article}\n", out);
   fputs("\\usepackage[margin=25mm]{geometry}\n", out);
   fputs("\\usepackage{graphicx}\n", out);
   // For pdfLaTeX compilation only:
   // fputs("\\usepackage[T1]{fontenc}\n", out);
   // fputs("\\usepackage[utf8]{inputenc}\n", out);
   // fputs("\\usepackage{lmodern}\n", out);

   fputs("\\usepackage{fontspec}\n", out);
   fputs("\\setmainfont{Latin Modern Roman}\n", out);

   // Emoji font 
   // Linux:
   // fputs("\\newfontfamily\\emojifont{Noto Color Emoji}\n", out);
   // Windows:
   fputs("\\newfontfamily\\emojifont{Segoe UI Emoji}\n", out);

   fputs("\\DeclareTextFontCommand{\\emoji}{\\emojifont}\n", out);
   // fputs("\\usepackage{ragged2e}\n", out);
   // fputs("\\AtBeginDocument{\\RaggedRight}\n", out);
   fputs("\\setlength{\\emergencystretch}{3em}\n", out);
   fputs("\\begin{document}\n\n", out);

   char line[8192];
   while(fgets(line, (int)sizeof(line), in))
   {
      // Remove trailing newline/space early
      trimRight(line);

      // Suppress unwanted metadata lines
      if(startsWithIgnoreCase(line, "Type:"))
      {
         continue;
      }

      if(startsWithIgnoreCase(line, "Received:"))
      {
         continue;
      }

      if(startsWithIgnoreCase(line, "From:"))
      {
         stripPhoneFromFromLine(line);
      }

      // Keep original newline behaviour: we escape content but preserve line breaks
      if(startsWith(line, "Attachment:"))
      {
         char attName[MaxPathLen];
         char attMime[128];
         long long attBytes = -1;
         int hasName = 0;

         parseAttachmentLine(line, attName, sizeof(attName), attMime, sizeof(attMime), &attBytes, &hasName);

         int idx = -1;
         if(hasName)
         {
            idx = findAttachmentByExactName(&list, attName);
         }
         if(idx < 0 && attBytes >= 0)
         {
            idx = findAttachmentBySize(&list, attBytes, isImageMime(attMime));
         }

         if(idx >= 0)
         {
            list.items[idx].used = 1;

            char relPath[MaxPathLen];
            snprintf(relPath, sizeof(relPath), "attachments/%s", list.items[idx].fileName);

            if(isImageMime(attMime) || hasImageExtension(list.items[idx].fileName))
            {
               writeImageInclude(out, relPath);
            }
            else
            {
               writeNonImageAttachment(out, relPath);
            }
         }
         else
         {
            // Could not match: keep a note in output
            fputs("\n\\begin{quote}\n", out);
            fputs("\\textbf{Unmatched attachment placeholder:} ", out);
            writeLatexEscaped(out, line);
            fputs("\\end{quote}\n\n", out);
         }

         continue;
      }

      // Normal text line
      trimRight(line);

      if(line[0] == '\0')
      {
         fputs("\n\n", out);   // Paragraph break in LaTeX
      }
      else
      {
         writeLatexEscaped(out, line);
         fputs("\\\\\n", out); // Keep forced line breaks only for non-empty lines
      }
   }

   fputs("\n\\end{document}\n", out);

   fclose(out);
   fclose(in);

   attachmentListFree(&list);

   fprintf(stderr, "Wrote %s\n", outputPath);
   return 0;
}
