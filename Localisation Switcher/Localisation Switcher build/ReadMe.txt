Localisation Switcher v0.1
========================

Thank you for your interest! Localisation Switcher is a submod for the Victoria II CreatineCore Modding Toolkit DLL modification, that lets you change language settings however you see fit.

Vanilla natively supports 13 languages, yet the game is still quite inconvenient for translators. Each language reads its own strictly fixed cell, separated by ';', from a line in the game's .csv files, for example:

NZL;New Zealand;Nouvelle-Zélande;Neuseeland;;Nueva Zelanda;;;;;;;;;x

First comes the cell for English, followed by French, German, Polish (skipped), Spanish, and several more empty cells.

First and foremost, Localisation Switcher lets you swap the order in which cells are read. Want Polish to be read straight from the first cell? Easy!
The sub-mod also lets you set separate text values for files outside the \localisation folder: in culture files, army names, and even province and state renaming effects.

How does it work?
The DLL hooks into the game, altering the original read order and adding new effects

How to install:
---------------
1. Most importantly, install the latest version of CC MTK into your copy of the game. You can download it on GitHub https://github.com/artyom-kuznetsov/V2DLL_Victoria-2-Extension or use Unofficial Patch 3.05 on ModDB https://www.moddb.com/mods/victoria-2-unofficial-patch-305
2. After that, extract the contents of the archive into your game's \mod folder (for ex. Victoria II\mod)
3. Make sure that the LOCAL_MOD_CONFIG=1 and LOAD_DLL_FROM_MOD_FOLDER=1 settings are set in the z_CreatineCore\creatinecore_settings.ini file.
4. Now you can start the game and choose submod in the Victoria II launcher. ("Localisation Switcher 0.1" by default)

I trust that if you've come this far in modding, you won't have any questions about how to integrate Localisation Switcher into your own mod.

The submod does not replace any original files of either the game or CreatineCore.

How to use:
-----------

1. localisation_switcher.ini is located in the folder mod\Localisation Switcher 0.1\Localisation Switcher
----------------------------------------------------------------------------------------------------------

1.1 There are 13 slots determining which languages read values from .csv files and in what order.

slot_1 = l_english
slot_2 = l_french
slot_3 = l_german
slot_4 = l_polish
slot_5 = l_spanish
slot_6 = l_italian
slot_7 = l_swedish
slot_8 = l_czech
slot_9 = l_hungarian
slot_10 = l_dutch
slot_11 = l_portugese
slot_12 = l_russian
slot_13 = l_finnish

Swap the desired languages or replace the desired slot with a new language (like slot_1 = l_scottish). Localization strings are now read in the order you assigned to their slots.

1.2 Don't forget to change the language in the language line of your settings files in Documents\Paradox Interactive\Victoria II and, if present, in the main mod/game folder.

language=l_scottish

2. How to use it in files outside \localisation
-----------------------------------------------

2.1 common\countries\country_name.txt

Country files in common\countries contain unit names that are assigned upon unit creation. Here, too, you can change the localization depending on the language. To do this, you need to define unit names inside a block named after the language. If a language not specified in the block is selected, values are read from the default list. For example:

unit_names = {
	manowar = {
		"Habsburg" "Viribus Unitis" "Ferdinand Max" "Aquila" "Montecuccoli" "Sterneck"
		"Kaiserin Maria Theresia" "Radetzky"
		l_scottish = {
			"Ye Habsburg" "Ye Viribus Unitis" "Ye Ferdinand Max"
		}
		l_french = {
			"Le Habsbourge" "Le Viribus Unitis" "Le Ferdinand Max"
		}
	}
}

2.2 common\cultures.txt

The cultures.txt file contains first and last names for leaders of various cultures, and now these can also be changed depending on the selected language. If a language not specified in the block is selected, values are read from the default list.

	ashkenazi = { 
		color = { 130 100 100}
		first_names = { Abraham Albert Benjamin Daniel David Franz Gustav Heinrich Hermann Isaak Ignatz Jakob Jeremias Josef Leonhard Michael Mose Salomon Saul Shmuel
		Sigmund Simon Theodor }

		last_names = { Abramovitch Ashkenazi Bernstein Birnbaum Bloch Cantor Cohen Frank Friedlдnder Ginsberg Goldstein Horowitz Israel Jacobsohn Kagan Kaplan Klein
		Levin Luria Morgenstern Nathan Oren Rosenthal Sachs Schapiro Schwartz Weinstein Weiss }
		l_scottish = {
			first_names = {
				"Ye Jew" "Ye Another Jew" Monty
			}
			last_names = {
				"Ye Iceberg" "Ye Goldstein"
			}
		}
		l_french = {
			first_names = { "Le Juif" "Le Autre Juif" Jaquais }
			last_names = { "Le Iceberg" "Le Goldstein" }
		}
	}
	
2.3 change_province_name and change_region_name effects in events and decisions

In vanilla effects look like change_province_name = "New Name"
In Localisation Switcher it has change into block. For example:

change_province_name = {
	default = "New Name"			#Name if your language is not specified in other blocks
	l_scottish = "Ye New Name"		#Name if your language is l_scottish
	l_french = "Le New Name"		#Name if your language is l_french
}

Works the same way for change_region_name effect.

2.4 define_general and define_admiral effects in events and decisions

In vanilla:

	define_general = {
		name = "Robert E.Lee"
		personality = audacious
		background = war_college
	}

Using Localisation Switcher it will be:
	
	define_general = {
		name = {
			default = "Robert E.Lee"		#Name if your language is not specified in other blocks
			l_scottish = "Ye Bob E.Lee"		#Name if your language is l_scottish
			l_french = "Le Rober E.Liuoix"	#Name if your language is l_french
		}
		personality = audacious
		background = war_college
	}
	
define_admiral works the same way

2.5 _oob files in history\units

Order of Battle files store the names of armies, navies, and leaders at the start of the game. To change how names load depending on your language, you again need to use a block named after that language, while also turning name into a block. For example:

navy = {
	name = {
		default = "Channel Fleet"
		l_scottish = "Ye English Basterds Fleet"
		l_french = "Le La Manche Fleet"
	}
	location = 303
	ship = {
		name = {
			default = "HMS Asia"
			l_scottish = "Ye Asia"
			l_french = "Le Asia"
		}
		type = manowar
	}
}

army = {
	name = {
		default = "1st Corps"
		l_scottish = "Bloody 1st"
		l_french = "Le 1. Corps" 
	}
	location = 300
	regiment = {
		name = {
			default = "Light Brigade"
			l_scottish = "Ye Too Light Btrigade"
			l_french = "Le Light Brigade"
		}
		type = hussar
		home = 300
	}
}

leader = {
	name = {
		default = "Thomas Cochrane"
		l_scottish = "Ye Tom Cochrane"
		l_french = "Le Thomas Cochrane"
	}
	date = 1832.1.1
	type = sea
	personality = charismatic
	background = adventurer
	prestige = 0.1
}

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