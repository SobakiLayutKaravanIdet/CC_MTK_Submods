TTF Unicode Support v0.2
========================

Thank you for your interest! TTF Unicode Support is a submod for the Victoria II CreatineCore Modding Toolkit DLL modification, enabling the use of Unicode fonts in .ttf format in your game.

In vanilla, the game relies on legacy .fnt fonts, which are very inconvenient for player customization and, most importantly, pose huge difficulties for translators. TTF Unicode Support fixes this issue, allowing you to use any custom .ttf fonts with support for whatever characters you need.

How does it work?
The DLL alters engine functionality on multiple levels: basic UI text elements render directly using .ttf fonts, while for country and province map labels, it automatically generates a raster counterpart from your font suitable for rendering.

How to install:
---------------
1. Most importantly, install the latest version of CC MTK into your copy of the game. You can download it on GitHub https://github.com/artyom-kuznetsov/V2DLL_Victoria-2-Extension or use Unofficial Patch 3.05 on ModDB https://www.moddb.com/mods/victoria-2-unofficial-patch-305
2. After that, extract the contents of the archive into your game's \mod folder (for ex. Victoria II\mod)
3. Make sure that the LOCAL_MOD_CONFIG=1 and LOAD_DLL_FROM_MOD_FOLDER=1 settings are set in the z_CreatineCore\creatinecore_settings.ini file.
4.1 If you are installing the submod standalone, copy the creatinecore_settings.ini file into the mod\TTF Unicode Support Mod folder and add the line TTF_UNICODE_SUPPORT=1 anywhere in the file.
4.2 If you are integrating TTF Unicode Support into your own submod, simply add the line TTF_UNICODE_SUPPORT=1 to your .ini file.
5. Now you can start the game and choose submod in the Victoria II launcher. ("TTF Unicode Support v0.2" by default)

The submod does not replace any original files of either the game or CreatineCore. In certain cases, the submod will create the ttf_unicode_support.gfx file in your mod's \interface folder (or create it if it doesn't exist).

How to use:
-----------

1. The ttf_settings.txt file located in the TTF Unicode Support Mod\TTF Unicode Support folder controls font configuration and its integration to the game.
2. It contains three block types: main_mapfont, provincename_mapfont, and gui_font
- main_mapfont defines which font will be used to display country names on the map.
- provincename_mapfont defines the font used for province names in-game.
- gui_font enables .ttf font integration across various other UI components (such as army names, decision text etc.).
3. The general principle is simple: place the necessary .ttf font files into the TTF Unicode Support Mod\TTF Unicode Support folder. After that, link the font to the required block or blocks in ttf_settings.txt (Make sure your downloaded font contains ALL the characters you need.)

3.1 How to define main_mapfont:

main_mapfont = {
    active = no	###no by default (loads vanilla fonts for country map labels). yes enables your chosen font.
    user_ttf_file = "YOUR_FONT.ttf"	###Insert the full font filename in quotes to display country names in this font.
    reserve_font_file = none  ###none by default. Insert the full filename of another font in quotes. If the main font is missing any required characters, they will be rendered from this fallback font (fully optional).
    color = 0 0 0	###0 0 0 by default (black). Set the font color by specifying its R G B values.
    height = 56		###Vanilla's 56 by default. The base font size for country names on the map. Change with caution.
    small_height = 22	###Vanilla's 22 by default. Small font size for country names on the map (for ex. vanilla's Ottoman Africa on start of the game) Change with caution.
}

3.2 How to define provincename_mapfont

provincename_mapfont = {
    active = no ###no by default (loads vanilla fonts for province names labels). yes enables your chosen font.
    user_ttf_file = "YOUR_FONT.ttf" ###Insert the full font filename in quotes to display province names in this font.
    color = 0 0 0 ###0 0 0 by default (black). Set the font color by specifying its R G B values.
}

The font size cannot be adjusted because font sizes for each province are defined in map\positions.txt

3.3 How to define gui_font

gui_font is commented out by default and serves as an example. To add a new .ttf font, you need to write a new gui_font block for each of its variations.

gui_font = {
	user_ttf_file = "YOUR_FONT.ttf"	###Insert the full font filename in quotes to use font in UI.
	gui_font_name = "my_ttf_font"	###A custom font name for use in .gui files.
	color = 0 0 0	###0 0 0 by default (black). Set the font color by specifying its R G B values.
	height = 18	###Specify the font size you will use. (It is recommended to take size values from the vanilla fonts you plan to replace).
	anti_aliasing = no ###no by default. Use "yes" to enable ClearType font processing and smoothing for a better visual effect (otherwise, the fonts will look very pixelated).
}

How to use:
Define the settings of the new gui_font block and use the value from gui_font_name to replace game fonts in the "font =" lines within the original game interface .gui files.

For example:
You want to change event description font (EVTDESCXXX;;x) to Comic Sans (...)

You should add comic sans .ttf file to \TTF Unicode Support folder

Then add new gui_font block to the ttf_settings.txt

gui_font = {
	user_ttf_file = "Comic sans.ttf"
	gui_font_name = "awful_comic_sans"	###Name can be fully custom
	color = 109 10 10	###You want it blood-red for some reason
	height = 18	###Because it will replace the vic_18_black font used in event description
	anti_aliasing = yes ###You want it smooth
}

Then you need to find a block for event description in game interface files. It will be a block in the event_country_window.gui:

instantTextBoxType = {
	name = "Description"
	position = { x = 26 y = 315 }
	textureFile = ""
	font = "vic_18_black"
	borderSize = {x = 0 y = 0}
	text = "Long text here!"	
	maxWidth = 534
	maxHeight = 205
	format = centre
}

So all you have to do - change line font = "vic_18_black" to font = "awful_comic_sans". Now your event descriptions will appear in awful Comic Sans.

Notice
-------

- The submod was tested on Cyrillic and Latin alphabets, including German, French, and Spanish layouts. Testing with other alphabets has not been performed.
- Multiplayer stability has not been tested and is not guaranteed.
- The impact on RAM usage has not been tested. Performance degradation is possible when integrated into large mods.
- Font usage has been tested on country map names, provinces, and some interface elements. Army and navy names have also been tested in the editor, but proper functionality is not guaranteed with multiplayer chat and other text input fields.

Credits
-------

- Created by $COUNTRY_ADJ$
Check out for new releases (and maybe even new submods) on ModDB
https://www.moddb.com/members/country-adj

- Based on CreatineCore Modding Toolkit by balticšprott
Discord Server of CC MTK: https://discord.gg/kkypeQukWF
GitHub: https://github.com/artyom-kuznetsov/V2DLL_Victoria-2-Extension

- Unofficial Patch 3.05 by maxioten 
ModDB: https://www.moddb.com/mods/victoria-2-unofficial-patch-305

Feel free to leave a feedback on CC MTK Discord Server!
