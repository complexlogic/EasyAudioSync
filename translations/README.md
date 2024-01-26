# Translation
Easy Audio Sync uses the Qt translation system for localization. To translate this application you will need to install Qt Linguist. On Linux, this is typically packaged as `qt-tools` or similar. For Windows users, there are third party providers of standalone Linguist builds, so you don't need to install the entire Qt development suite, which is quite large. See [here](https://github.com/thurask/Qt-Linguist), for example.

## Instructions
Determine your language's [ISO-639](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes) code. Region extensions are supported. For example, Portuguese is `pt` and Brazillian Portuguese is `pt_BR`.

### Qt Linguist
The translation source files have `.ts` file extension. Use the `source.ts` file as a base. Open it in Qt Linguist and save it as a new file with your language's ISO-639 code. Click Edit->Translation File Settings, and set the target language.

Translate all of the application's strings and mark them with the green "done" checkmark. Strings may have format specifiers such as `%1`, `%2`, etc. This means that the application will substitute values at runtime in the order given. It's important to keep these format specifiers in the translated string, but you may need to change the order based on your language's grammar.

### Languages File
After you have translated the strings in Qt Linguist, open the `languages.txt` file in a text editor. Add a line which contains your language's ISO-639 code and the name of the language (in the language itself), delimited by a semicolon without any space between. The languages should be in alphabetical order based on the ISO-639 code. The `languages.txt` file triggers the compilation of the source files and populates the Language combo box in the application settings dialog.

For example, a `languages.txt` file containing English, Spanish and German should look like this:
```
de;Deutsch
en;English
es;Español
```
