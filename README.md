# terminal-cancer
Makes your terminal 1773 w1th c001 h4xx0rz, or pl4ys 4nnoy1ng t3xt2sp33ch m355agez 4s r3sp0ns3 to 3x3cut3d c0mm4ndz

I took some inspiration from this thread here: https://stackoverflow.com/questions/6075013
I'm pretty sure nobody will ever attempt to runthis
### requirements to run
1. Linux
2. probably some amount of tweaking (check the source code):
  * edit the variables so they point to existing filepaths (pretty sure no1 haz defined **$OWN_STUFF** on their devices :)
  * in the directory noted by variable "**preset_path**", add/or generate mp3 files that are supposed to be played
    * file named "**cat.mp3**" will be played on execution of **cat** command, "**ls.mp3**" would be played on **ls** command, and so on
    * example generator script that I used is attached in the directory
    * for attached generator script to work, I installed globally https://pypi.org/project/gTTS/ , otherwise `tts` command prolly won't work
    * **tts** script is basically bas wrapper for gTTS package
  * make sure your favourite terminal is present in "**listened-ternimal-types**" variable, if nah, find what it's called when started as process (use `top` or sth to find out) 
