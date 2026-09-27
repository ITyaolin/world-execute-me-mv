/* Hand-authored ASCII-art sprites for the mili music-video player.
 * Ink characters come from the density ramp " .:-=+*#%@" (low to
 * high); a space is transparent.  Every row is exactly `w` bytes
 * long, and every sprite is drawn about twice as wide as it is
 * tall so that it reads correctly in a 2:1 terminal cell. */
#include "sprites.h"
#include <string.h>

/* eggplant: aubergine with a leafy stem, tilted slightly */
static const char *const SP_EGGPLANT[] = {
    "     .-  :=  -. ",
    "   .:-=+*#%#*=-:",
    "  .:=+*#%@@%#*=:",
    "    -#%%%*-     ",
    "   +@@%%%##+    ",
    "  :%@%%#####*   ",
    "  :%%%####***.  ",
    "   -#####***+   ",
    "    .-****+:    ",
    NULL
};

/* tomato: round tomato with a small star-shaped calyx on top */
static const char *const SP_TOMATO[] = {
    "   ##:-##-:##  ",
    "  ..########.. ",
    " :=+++*@@*====.",
    " ==####==####=-",
    " ==.========.=-",
    " :==========--.",
    "  .-======--:  ",
    "     .::::.    ",
    NULL
};

/* cat: sitting tabby cat, front view, ears + tail, stripe texture */
static const char *const SP_CAT[] = {
    "      ##      ##      ",
    "     .%.######.%.     ",
    "     ###%%++%%### **. ",
    "      @.%%..%%.@ .#%%*",
    "      ##++++++##  .%%#",
    "       ###..###    *%%",
    "      ...+..+...  .%%#",
    "      #..+..+..***%%%.",
    "     ##*##..##******. ",
    NULL
};

/* god: all-seeing eye inside radiating rays / halo */
static const char *const SP_GOD[] = {
    " *  *.    **    .*  *",
    " *=- -=.  ==  .=- -=*",
    "  -+- -+- == -+- -+- ",
    "   .-***++++++***-.  ",
    "  .***.#%##..%#.***. ",
    "  *#*++%%....%%++*#* ",
    "   .***+*%..%*+***.  ",
    "   -+-.***++***.-+-  ",
    " .==.-=-  ==  -=-.==.",
    ".*- .+-   ==   -+. -*",
    "    **    **    **   ",
    NULL
};

/* female: Venus symbol: circle with a cross below */
static const char *const SP_FEMALE[] = {
    "    :---:    ",
    "  -**===**-  ",
    " .#*     *#. ",
    " .*#:   :#*. ",
    "  .=*#*#*=.  ",
    "     -#-     ",
    "     -#-     ",
    "     -#-     ",
    "  =#######=  ",
    "     -#-     ",
    NULL
};

/* male: Mars symbol: circle with an arrow going up-right */
static const char *const SP_MALE[] = {
    "             ",
    "            .",
    "         .+**",
    "         =#=.",
    "  .-===-+#:  ",
    " -#*---*#-   ",
    " ##.   .##   ",
    " -#*---*#-   ",
    "  .-===-.    ",
    "             ",
    NULL
};

/* chain: two interlocking chain links */
static const char *const SP_CHAIN[] = {
    "  :=+==+=:   :=+==+=: ",
    ".++:    .=*. :.    :++",
    "*=        -*.        =",
    "#:        :#.        :",
    "*=        .*-        =",
    ".++:    .: .*=.    :++",
    "  :=+==+=:   :=+==+=: ",
    NULL
};

/* spiral: inward-winding spiral / vortex */
static const char *const SP_SPIRAL[] = {
    "                   ",
    "         @@@@@     ",
    "             @@@   ",
    "     %%######  @@@ ",
    "   %%%      ##   @ ",
    "   %   ####  ##  @@",
    "  %%  ##  #  ##  @@",
    "   %   #######  %% ",
    "   %%%         %%  ",
    "     %%%%%%%%%%%   ",
    "                   ",
    NULL
};

/* heart: classic heart */
static const char *const SP_HEART[] = {
    " :=++=. .=++=: ",
    "-##%%%#=######:",
    "-##%%%%######*-",
    " =##%%#####**- ",
    "  :*#####**+.  ",
    "    =##***-    ",
    "     :**+.     ",
    "      **+      ",
    NULL
};

/* heart_broken: heart split by a jagged crack */
static const char *const SP_HEART_BROKEN[] = {
    " -*##*- -*##*- ",
    "-###%%- -#####-",
    "=##%%- -#####*=",
    ".*#%%%- -###*+ ",
    "  +###- -#**=  ",
    "   =###- -*-   ",
    "    -*- -*:    ",
    "     .- -      ",
    "      - -      ",
    NULL
};

/* warning: triangular warning sign with an exclamation mark inside */
static const char *const SP_WARNING[] = {
    "        +        ",
    "       *#*       ",
    "      ++*++      ",
    "     =**@**=     ",
    "    =*-*@*-*=    ",
    "   .#--*@*--#.   ",
    "  .#.--.*.--.#.  ",
    "  *.---*%*---.*  ",
    " *@@@@@@@@@@@@@* ",
    NULL
};

/* clock: round clock face with hands and tick marks */
static const char *const SP_CLOCK[] = {
    "       :::::       ",
    "   .=#%#***#%#=.   ",
    "  +%*-- *@* --*%+  ",
    " *%--   *@*   --%* ",
    "-%+     *#****  +%-",
    "-%-=-   ###****==%-",
    ".%#--         --#%.",
    " -%-: -  -  - :-%- ",
    "  .+%---:*:---%+.  ",
    "     -=+***+=-     ",
    NULL
};

/* plug: mains plug with two prongs and a cable */
static const char *const SP_PLUG[] = {
    "       ++    ++      ",
    "       @@    @@      ",
    "       **    **      ",
    "      -*%%####+-     ",
    "     +#%%%######+    ",
    "     #########***    ",
    "     =#####*****-    ",
    "      :=*.==.+-      ",
    "          .=#********",
    NULL
};

/* shield: heraldic shield / protective barrier */
static const char *const SP_SHIELD[] = {
    ".#*+++++++++*#.",
    "=@===========@=",
    "=@===========@=",
    "=@====.=.====@=",
    " @==.+%%%+.==@ ",
    " %=*%*===*%*=% ",
    " =+=.=====.=+= ",
    "  #*=====--*#  ",
    "   **===--**   ",
    "    =+*#*+=    ",
    NULL
};

/* cage: bars of a cage in perspective */
static const char *const SP_CAGE[] = {
    "     ------------------",
    "   - - -   -   -   -  -",
    "  -  --   -   -   -   -",
    "#===#===#===#===#-    -",
    "#   #   #   #   #     -",
    "#   #   #   #   #     -",
    "#   #---#---#---#------",
    "#   #   #   #   #   -  ",
    "#   #   #   #   # -    ",
    "#===#===#===#===#      ",
    NULL
};

/* smile: smiling face, eyes and a wide happy mouth */
static const char *const SP_SMILE[] = {
    "   :=+***+=:   ",
    " :*%+-:::-+%*: ",
    ":%#.**   **.#%:",
    "*%: *#. .#* :%*",
    "*%.**     **.%*",
    ":%#*@#***#@*#%:",
    " :*%.*****.%*: ",
    "   :=+***+=:   ",
    NULL
};

/* question: a large question mark */
static const char *const SP_QUESTION[] = {
    "    +@@@#+. ",
    "   *@@##@@= ",
    "   +#+=%@@+ ",
    "     #@@#=  ",
    "     #@@=   ",
    "     @@@    ",
    "     @@@    ",
    "     .*.    ",
    "     %@%    ",
    "     .*.    ",
    NULL
};

/* terminal: CRT monitor / code terminal window with a prompt line */
static const char *const SP_TERMINAL[] = {
    "    .=+*+++++++++*+=.   ",
    "  =*.................*= ",
    " =*.:-==-:....:-=+*#*.*=",
    " @..*-=+*#*=:...:-+*#*.@",
    " @...:=+*#*=-:....=-:..@",
    " =*.-=+#*=:...@@@@@...*=",
    "  =*:-+*#*=-:....:=+**= ",
    "    .=+*+++++++++*+=.   ",
    "         .=   =.        ",
    "        -+++++++-       ",
    "          .....         ",
    NULL
};

const MvSprite MV_SPRITES[] = {
    { "eggplant", SP_EGGPLANT, 16, 9 },
    { "tomato", SP_TOMATO, 15, 8 },
    { "cat", SP_CAT, 22, 9 },
    { "god", SP_GOD, 21, 11 },
    { "female", SP_FEMALE, 13, 10 },
    { "male", SP_MALE, 13, 10 },
    { "chain", SP_CHAIN, 22, 7 },
    { "spiral", SP_SPIRAL, 19, 11 },
    { "heart", SP_HEART, 15, 8 },
    { "heart_broken", SP_HEART_BROKEN, 15, 9 },
    { "warning", SP_WARNING, 17, 9 },
    { "clock", SP_CLOCK, 19, 10 },
    { "plug", SP_PLUG, 21, 9 },
    { "shield", SP_SHIELD, 15, 10 },
    { "cage", SP_CAGE, 23, 10 },
    { "smile", SP_SMILE, 15, 8 },
    { "question", SP_QUESTION, 12, 10 },
    { "terminal", SP_TERMINAL, 24, 11 },
};

const int MV_SPRITE_COUNT =
    (int)(sizeof(MV_SPRITES) / sizeof(MV_SPRITES[0]));

const MvSprite *mv_sprite(const char *name)
{
    int i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < MV_SPRITE_COUNT; ++i) {
        if (strcmp(MV_SPRITES[i].name, name) == 0) {
            return &MV_SPRITES[i];
        }
    }
    return NULL;
}
