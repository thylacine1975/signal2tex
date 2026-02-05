# signal2tex

This tool converts Signal Desktop conversation exports into LaTeX documents, merging graphical attachments inline with message text. It accompanies the [sigtop](https://github.com/tbvdm/sigtop) utility for exporting Signal conversations.

## Operation

### Step 1: Export Signal Conversation Data

First, export the text messages from your Signal conversation using sigtop:

```bash
sigtop msg -c MySignalConversation <subdirectory to write to>
```

This exports the conversation messages to a text file in the specified subdirectory.

### Step 2: Export Attachments

Next, export the attachments from the same conversation:

```bash
sigtop att -c MySignalConversation attachments
```

This creates an `attachments` directory containing all files shared in the conversation.

### Step 3: Convert to LaTeX

Run `txt2tex` on the exported message file:

```bash
txt2tex <input_file>
```

The program:
- Reads the exported text file line by line
- Processes attachment references and matches them with files in the `./attachments` directory by filename or file size
- Escapes special LaTeX characters in text content
- Handles UTF-8 characters and emojis using appropriate LaTeX commands
- Filters out unwanted metadata lines (Type:, Received:, etc.)
- Strips phone numbers from "From:" lines
- Generates a complete LaTeX document with proper preamble and formatting

Images are included using `\includegraphics`, while non-image attachments are listed as text references. The output file has the same name as the input file but with a `.tex` extension.

### Step 4: Compile LaTeX

Compile the generated `.tex` file using LuaLaTeX manually: 

```bash
lualatex output.tex
```

...or graphically using [TeXstudio](https://www.texstudio.org).
(Don't forget to select LuaLaTeX as the default compiler in the TeXstudio build options for the correct rendering of emoji.)
