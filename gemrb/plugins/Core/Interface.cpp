/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003-2005 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/plugins/Core/Interface.cpp,v 1.343 2005/08/14 19:07:26 avenger_teambg Exp $
 *
 */

#ifndef INTERFACE
#define INTERFACE
#endif

#include <config.h>

#include <stdlib.h>
#include <time.h>

#ifdef WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#endif

#include "Interface.h"
#include "FileStream.h"
#include "AnimationMgr.h"
#include "ArchiveImporter.h"
#include "WorldMapMgr.h"
#include "AmbientMgr.h"
#include "ItemMgr.h"
#include "SpellMgr.h"
#include "EffectMgr.h"
#include "StoreMgr.h"
#include "DialogMgr.h"
#include "MapControl.h"
#include "EffectQueue.h"
#include "MapMgr.h"

GEM_EXPORT Interface* core;

#ifdef WIN32
GEM_EXPORT HANDLE hConsole;
#endif

#include "../../includes/win32def.h"
#include "../../includes/globals.h"

//use DialogF.tlk if the protagonist is female, that's why we leave space
static char dialogtlk[] = "dialog.tlk\0";
#define STRREFCOUNT 100
static int strref_table[STRREFCOUNT];

Interface::Interface(int iargc, char** iargv)
{
	argc = iargc;
	argv = iargv;
#ifdef WIN32
	hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
#endif
	textcolor( LIGHT_WHITE );
	printf( "GemRB Core Version v%s Loading...\n", VERSION_GEMRB );
	video = NULL;
	key = NULL;
	strings = NULL;
	guiscript = NULL;
	windowmgr = NULL;
	vars = NULL;
	tokens = NULL;
	RtRows = NULL;
	music = NULL;
	soundmgr = NULL;
	sgiterator = NULL;
	INIparty = NULL;
	INIbeasts = NULL;
	INIquests = NULL;
	game = NULL;
	worldmap = NULL;
	CurrentStore = NULL;
	CurrentContainer = NULL;
	UseContainer = false;
	timer = NULL;
	evntmgr = NULL;
	console = NULL;
	slottypes = NULL;
	slotmatrix = NULL;
	ModalWindow = NULL;
	tooltip_x = 0;
	tooltip_y = 0;
	tooltip_ctrl = NULL;
	plugin = NULL;
	factory = NULL;
	
	pal16 = NULL;
	pal32 = NULL;
	pal256 = NULL;

	CursorCount = 0;
	Cursors = NULL;

	ConsolePopped = false;
	CheatFlag = false;
	FogOfWar = 1;
	QuitFlag = QF_NORMAL;
#ifndef WIN32
	CaseSensitive = true; //this is the default value, so CD1/CD2 will be resolved
#else
	CaseSensitive = false;
#endif
	GameOnCD = false;
	SkipIntroVideos = false;
	DrawFPS = false;
	TooltipDelay = 100;
	GUIScriptsPath[0] = 0;
	GamePath[0] = 0;
	SavePath[0] = 0;
	GemRBPath[0] = 0;
	PluginsPath[0] = 0;
	GameName[0] = 0;
	strncpy( GameOverride, "override", sizeof(GameOverride) );
	strncpy( GameSounds, "sounds", sizeof(GameSounds) );
	strncpy( GameScripts, "scripts", sizeof(GameScripts) );
	strncpy( GameData, "data", sizeof(GameData) );
	strncpy( INIConfig, "baldur.ini", sizeof(INIConfig) );
	strncpy( ButtonFont, "STONESML", sizeof(ButtonFont) );
	strncpy( TooltipFont, "STONESML", sizeof(TooltipFont) );
	strncpy( CursorBam, "CAROT", sizeof(CursorBam) );
	strncpy( GlobalScript, "BALDUR", sizeof(GlobalScript) );
	strncpy( WorldMapName, "WORLDMAP", sizeof(WorldMapName) );
	strncpy( Palette16, "MPALETTE", sizeof(Palette16) );
	strncpy( Palette32, "PAL32", sizeof(Palette32) );
	strncpy( Palette256, "MPAL256", sizeof(Palette256) );
	strcpy( TooltipBackResRef, "\0" );
	TooltipColor.r = 0;
	TooltipColor.g = 255;
	TooltipColor.b = 0;
	TooltipColor.a = 255;
	TooltipMargin = 10;
	TooltipBack = NULL;
	DraggedItem = NULL;
	DefSound = NULL;
	DSCount = -1;
	for(unsigned int i=0;i<sizeof(FogSprites)/sizeof(Sprite2D *);i++ ) FogSprites[i]=NULL;
	GameFeatures = 0;
	memset( WindowFrames, 0, sizeof( WindowFrames ));
}

#define FreeInterfaceVector(type, variable, member) \
{ \
	std::vector<type>::iterator i; \
	for(i = variable.begin(); i != variable.end(); ++i) { \
	if (!(*i).free) { \
		FreeInterface((*i).member); \
		(*i).free = true; \
	} \
	} \
}

#define FreeResourceVector(type, variable) \
{ \
	unsigned int i=variable.size(); \
	while(i--) { \
		if (variable[i]) { \
			delete variable[i]; \
		} \
	} \
	variable.clear(); \
}

static void ReleaseItem(void *poi)
{
	delete ((Item *) poi);
}

static void ReleaseSpell(void *poi)
{
	delete ((Spell *) poi);
}

static void ReleaseEffect(void *poi)
{
	delete ((Effect *) poi);
}

Interface::~Interface(void)
{
	//destroy the highest objects in the hierarchy first!
	if (game) {
		delete( game );
	}
	if (worldmap) {
		delete( worldmap );
	}
	CharAnimations::ReleaseMemory();
	if (CurrentStore) {
		delete CurrentStore;
	}
	ItemCache.RemoveAll(ReleaseItem);
	SpellCache.RemoveAll(ReleaseSpell);
	EffectCache.RemoveAll(ReleaseEffect);
	if (DefSound) {
		free( DefSound );
		DSCount = -1;
	}

	if (slottypes) {
		free( slottypes );
	}
	if (slotmatrix) {
		free( slotmatrix );
	}
	if (music) {
		FreeInterface( music );
	}
	if (soundmgr) {
		FreeInterface( soundmgr );
	}
	if (sgiterator) {
		delete( sgiterator );
	}
	if (factory) {
		delete( factory );
	}
	if (Cursors) {
		for (int i = 0; i < CursorCount; i++) {
			//freesprite doesn't free NULL
			video->FreeSprite( Cursors[i] );
		}
		delete[] Cursors;
	}

	FreeResourceVector( Font, fonts );
	FreeResourceVector( Window, windows );
	if (console) {
		delete( console );
	}

	if (key) {
		FreeInterface( key );
	}	
	if (strings) {
		FreeInterface( strings );
	}
	if (pal256) {
		FreeInterface( pal256 );
	}
	if (pal32) {
		FreeInterface( pal32 );
	}
	if (pal16) {
		FreeInterface( pal16 );
	}

	if (timer) {
		delete( timer );
	}

	if (windowmgr) {
		FreeInterface( windowmgr );
	}

	if (video) {
		unsigned int i;
		
		for(i=0;i<sizeof(FogSprites)/sizeof(Sprite2D *);i++ ) {
			//freesprite checks for null pointer
			video->FreeSprite(FogSprites[i]);
		}
		for(i=0;i<4;i++) {
			video->FreeSprite(WindowFrames[i]);
		}
		
		if (TooltipBack) {
			for(i=0;i<3;i++) {
				//freesprite checks for null pointer
				video->FreeSprite(TooltipBack[i]);
			}
			delete[] TooltipBack;
		}
		FreeInterface( video );
	}

	if (evntmgr) {
		delete( evntmgr );
	}
	if (guiscript) {
		FreeInterface( guiscript );
	}
	if (vars) {
		delete( vars );
	}
	if (tokens) {
		delete( tokens );
	}
	if (RtRows) {
		delete( RtRows );
	}
	FreeInterfaceVector( Table, tables, tm );
	FreeInterfaceVector( Symbol, symbols, sm );

	if (INIquests) {
		FreeInterface(INIquests);
	}
	if (INIbeasts) {
		FreeInterface(INIbeasts);
	}
	if (INIparty) {
		FreeInterface(INIparty);
	}
	Map::ReleaseMemory();
	delete( plugin );
	// Removing all stuff from Cache, except bifs
	DelTree((const char *) CachePath, true);
}

GameControl* Interface::StartGameControl()
{
	//making sure that our window is the first one
	if (ConsolePopped) {
		PopupConsole();
	}
	DelWindow(~0); //deleting ALL windows
	DelTable(~0);  //dropping ALL tables
	Window* gamewin = new Window( 0xffff, 0, 0, Width, Height );
	GameControl* gc = new GameControl();
	gc->XPos = 0;
	gc->YPos = 0;
	gc->Width = Width;
	gc->Height = Height;
	gc->Owner = gamewin;
	gc->ControlID = 0x00000000;
	gc->ControlType = IE_GUI_GAMECONTROL;
	gamewin->AddControl( gc );
	AddWindow( gamewin );
	SetVisible( 0, WINDOW_VISIBLE );
	//setting the focus to the game control 
	evntmgr->SetFocused(gamewin, gc);
	if (guiscript->LoadScript( "MessageWindow" )) {
		guiscript->RunFunction( "OnLoad" );
		gc->UnhideGUI();
	}

	return gc;
}

/* handle main loop events that might destroy or create windows 
	 thus cannot be called from DrawWindows directly
 */
void Interface::HandleFlags()
{
	if (QuitFlag&(QF_QUITGAME|QF_EXITGAME) ) {
		// when reaching this, quitflag should be 1 or 2
		// if Exitgame was set, we'll set a new script
			QuitGame (QuitFlag!=QF_QUITGAME);
			QuitFlag &= ~(QF_QUITGAME|QF_EXITGAME);
	}
	
	if (QuitFlag&QF_LOADGAME) {
		QuitFlag &= ~QF_LOADGAME;
		LoadGame(LoadGameIndex);
	}
	
	if (QuitFlag&QF_ENTERGAME) {
		QuitFlag &= ~QF_ENTERGAME;
		if (game) {
			GameControl* gc = StartGameControl();
			Actor* actor = game->GetPC (0, false);
			gc->ChangeMap(actor, true);
		} else {
			printMessage("Core", "No game to enter...", LIGHT_RED );
			QuitFlag = QF_QUITGAME;
		}
	}
	
	if (QuitFlag&QF_CHANGESCRIPT) {
		QuitFlag &= ~QF_CHANGESCRIPT;
		guiscript->LoadScript( NextScript );			
		guiscript->RunFunction( "OnLoad" );
	}
}

/** this is the main loop */
void Interface::Main()
{
	video->CreateDisplay( Width, Height, Bpp, FullScreen );
	video->SetDisplayTitle( GameName, GameType );
	Font* fps = GetFont( ( unsigned int ) 0 );
	char fpsstring[_MAX_PATH];
	Color fpscolor = {0xff,0xff,0xff,0xff}, fpsblack = {0x00,0x00,0x00,0xff};
	unsigned long frame = 0, time, timebase;
	GetTime(timebase);
	double frames = 0.0;
	Region bg( 0, 0, 100, 30 );
	Color* palette = video->CreatePalette( fpscolor, fpsblack );
	do {
		//don't change script when quitting is pending

		while (QuitFlag) {
			HandleFlags();
		}

		DrawWindows();
		if (DrawFPS) {
			frame++;
			GetTime( time );
			if (time - timebase > 1000) {
				frames = ( frame * 1000.0 / ( time - timebase ) );
				timebase = time;
				frame = 0;
				sprintf( fpsstring, "%.3f fps", frames );
			}
			video->DrawRect( bg, fpsblack );
			fps->Print( Region( 0, 0, 100, 20 ),
						( unsigned char * ) fpsstring, palette,
						IE_FONT_ALIGN_LEFT | IE_FONT_ALIGN_MIDDLE, true );
		}
	} while (video->SwapBuffers() == GEM_OK);
	video->FreePalette( palette );
}

bool Interface::ReadStrrefs()
{
	int i;
	TableMgr * tab;
	int table=LoadTable("strings");
	memset(strref_table,-1,sizeof(strref_table) );
	if (table<0) {
		return false;
	}
	tab = GetTable(table);
	if (!tab) {
		goto end;
	}
	for(i=0;i<STRREFCOUNT;i++) {
		strref_table[i]=atoi(tab->QueryField(i,0));
	}
end:
	DelTable(table);
	return true;
}

int Interface::Init()
{
	printMessage( "Core", "Initializing Variables Dictionary...", WHITE );
	vars = new Variables();
	if (!vars) {
		printStatus( "ERROR", LIGHT_RED );
		return GEM_ERROR;
	}

	vars->SetAt( "Volume Ambients", 100 );
	vars->SetAt( "Volume Movie", 100 );
	vars->SetAt( "Volume Music", 100 );
	vars->SetAt( "Volume SFX", 100 );
	vars->SetAt( "Volume Voices", 100 );
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Loading Configuration File...", WHITE );
	if (!LoadConfig()) {
		printStatus( "ERROR", LIGHT_RED );
		printMessage( "Core",
			"Cannot Load Config File.\nTermination in Progress...\n", WHITE );
		exit( -1 );
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Starting Plugin Manager...\n", WHITE );
	plugin = new PluginMgr( PluginsPath );
	printMessage( "Core", "Plugin Loading Complete...", WHITE );
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Creating Object Factory...", WHITE );
	factory = new Factory();
	printStatus( "OK", LIGHT_GREEN );
	time_t t;
	t = time( NULL );
	srand( ( unsigned int ) t );
#ifdef _DEBUG
	FileStreamPtrCount = 0;
	CachedFileStreamPtrCount = 0;
#endif
	printMessage( "Core", "GemRB Core Initialization...\n", WHITE );
	printMessage( "Core", "Searching for Video Driver...", WHITE );
	if (!IsAvailable( IE_VIDEO_CLASS_ID )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "No Video Driver Available.\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Initializing Video Plugin...", WHITE );
	video = ( Video * ) GetInterface( IE_VIDEO_CLASS_ID );
	if (video->Init() == GEM_ERROR) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "Cannot Initialize Video Driver.\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Searching for KEY Importer...", WHITE );
	if (!IsAvailable( IE_KEY_CLASS_ID )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "No KEY Importer Available.\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Initializing Resource Manager...\n", WHITE );
	key = ( ResourceMgr * ) GetInterface( IE_KEY_CLASS_ID );
	char ChitinPath[_MAX_PATH];
	PathJoin( ChitinPath, GamePath, "chitin.key", NULL );
	ResolveFilePath( ChitinPath );
	if (!key->LoadResFile( ChitinPath )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "Cannot Load Chitin.key\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	if (!LoadGemRBINI())
	{
		printf( "Cannot Load INI\nTermination in Progress...\n" );
		return GEM_ERROR;
	}

	printMessage( "Core", "Checking for Dialogue Manager...", WHITE );
	if (!IsAvailable( IE_TLK_CLASS_ID )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "No TLK Importer Available.\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	printStatus( "OK", LIGHT_GREEN );
	strings = ( StringMgr * ) GetInterface( IE_TLK_CLASS_ID );
	printMessage( "Core", "Loading Dialog.tlk file...", WHITE );
	char strpath[_MAX_PATH];
	PathJoin( strpath, GamePath, dialogtlk, NULL );
	ResolveFilePath( strpath );
	FileStream* fs = new FileStream();
	if (!fs->Open( strpath, true )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "Cannot find Dialog.tlk.\nTermination in Progress...\n" );
		delete( fs );
		return GEM_ERROR;
	}
	printStatus( "OK", LIGHT_GREEN );
	strings->Open( fs, true );

	printMessage( "Core", "Loading Palettes...\n", WHITE );
	DataStream* bmppal16 = NULL;
	DataStream* bmppal32 = NULL;
	DataStream* bmppal256 = NULL;
	if (!IsAvailable( IE_BMP_CLASS_ID )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "No BMP Importer Available.\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	bmppal16 = key->GetResource( Palette16, IE_BMP_CLASS_ID );
	if (bmppal16) {
		pal16 = ( ImageMgr * ) GetInterface( IE_BMP_CLASS_ID );
		pal16->Open( bmppal16, true );
	} else {
		pal16 = NULL;
	}
	bmppal32 = key->GetResource( Palette32, IE_BMP_CLASS_ID );
	if (bmppal32) {
		pal32 = ( ImageMgr * ) GetInterface( IE_BMP_CLASS_ID );
		pal32->Open( bmppal32, true );
	} else {
		pal32 = NULL;
	}
	bmppal256 = key->GetResource( Palette256, IE_BMP_CLASS_ID );
	if (bmppal256) {
		pal256 = ( ImageMgr * ) GetInterface( IE_BMP_CLASS_ID );
		pal256->Open( bmppal256, true );
	} else {
		pal256 = NULL;
	}
	printMessage( "Core", "Palettes Loaded\n", WHITE );

	if (!IsAvailable( IE_BAM_CLASS_ID )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "No BAM Importer Available.\nTermination in Progress...\n" );
		return GEM_ERROR;
	}
	AnimationMgr* anim = ( AnimationMgr* ) GetInterface( IE_BAM_CLASS_ID );
	if (!anim) {
		printf( "No BAM Importer Available.\nTermination in Progress...\n" );
		return GEM_ERROR;
		
	}

	DataStream* str = NULL;
	int ret = GEM_ERROR;
	int table = -1;
	if (!IsAvailable( IE_2DA_CLASS_ID )) {
		printf( "No 2DA Importer Available.\nTermination in Progress...\n" );
		goto end_of_init;
	}

	printMessage( "Core", "Initializing stock sounds...", WHITE );
	table = LoadTable( "defsound" );
	if (table < 0) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "Cannot find defsound.2da.\nTermination in Progress...\n" );
		goto end_of_init;
	} else {
		TableMgr* tm = GetTable( table );
		if (tm) {
			DSCount = tm->GetRowCount();
			DefSound = (ieResRef *) calloc( DSCount, sizeof(ieResRef) );
			for (int i = 0; i < DSCount; i++) {
				strnuprcpy( DefSound[i], tm->QueryField( i, 0 ), 8 );
			}
			DelTable( table );
		}
	}

	printMessage( "Core", "Loading Fonts...\n", WHITE );
	table = LoadTable( "fonts" );
	if (table < 0) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "Cannot find fonts.2da.\nTermination in Progress...\n" );
		goto end_of_init;
	} else {
		TableMgr* tab = GetTable( table );
		int count = tab->GetRowCount();
		for (int i = 0; i < count; i++) {
			char* ResRef = tab->QueryField( i, 0 );
			int needpalette = atoi( tab->QueryField( i, 1 ) );
			int first_char = atoi( tab->QueryField( i, 2 ) );
			str = key->GetResource( ResRef, IE_BAM_CLASS_ID );
			if (!anim->Open( str, true )) {
// opening with autofree makes this delete unwanted!!!
//				delete( fstr );
				continue;
			}
			Font* fnt = anim->GetFont();
			if (!fnt) {
				continue;
			}
			strncpy( fnt->ResRef, ResRef, 8 );
			if (needpalette) {
				Color fore = {0xff, 0xff, 0xff, 0x00};
				Color back = {0x00, 0x00, 0x00, 0x00};
				if (!strnicmp( TooltipFont, ResRef, 8) ) {
					fore = TooltipColor;
				}
				Color* pal = video->CreatePalette( fore, back );
				memcpy( fnt->GetPalette(), pal, 256 * sizeof( Color ) );
				video->FreePalette( pal );
			}
			fnt->SetFirstChar( first_char );
			fonts.push_back( fnt );
		}
		DelTable( table );
	}
	printMessage( "Core", "Fonts Loaded...", WHITE );
	printStatus( "OK", LIGHT_GREEN );

	if (TooltipBackResRef[0]) {
		printMessage( "Core", "Initializing Tooltips...", WHITE );
		str = key->GetResource( TooltipBackResRef, IE_BAM_CLASS_ID );
		if (!anim->Open( str, true )) {
			printStatus( "ERROR", LIGHT_RED );
			goto end_of_init;
		}
		TooltipBack = new Sprite2D * [3];
		for (int i = 0; i < 3; i++) {
			TooltipBack[i] = anim->GetFrameFromCycle( i, 0 );
			TooltipBack[i]->XPos = 0;
			TooltipBack[i]->YPos = 0;
		}
		printStatus( "OK", LIGHT_GREEN );
	}

	printMessage( "Core", "Initializing the Event Manager...", WHITE );
	evntmgr = new EventMgr();
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "BroadCasting Event Manager...", WHITE );
	video->SetEventMgr( evntmgr );
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Initializing Window Manager...", WHITE );
	windowmgr = ( WindowMgr * ) GetInterface( IE_CHU_CLASS_ID );
	if (windowmgr == NULL) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "Core", "Initializing GUI Script Engine...", WHITE );
	guiscript = ( ScriptEngine * ) GetInterface( IE_GUI_SCRIPT_CLASS_ID );
	if (guiscript == NULL) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	if (!guiscript->Init()) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );
	strcpy( NextScript, "Start" );

	printMessage( "Core", "Setting up the Console...", WHITE );
	QuitFlag = QF_CHANGESCRIPT;
	console = new Console();
	console->XPos = 0;
	console->YPos = Height - 25;
	console->Width = Width;
	console->Height = 25;
	console->SetFont( fonts[0] );
	{
		Sprite2D *tmpsprite = GetCursorSprite();
		if (tmpsprite) {
			console->SetCursor (tmpsprite);
			printStatus( "OK", LIGHT_GREEN );
		} else {
			printStatus( "ERROR", LIGHT_GREEN );
		}
	}
/*
	str = key->GetResource( CursorBam, IE_BAM_CLASS_ID );
	if (anim->Open(str, true) ) {
		console->SetCursor( anim->GetFrameFromCycle( 0,0 ) );
	}
	else {
	}
*/
	printMessage( "Core", "Starting up the Sound Manager...", WHITE );
	soundmgr = ( SoundMgr * ) GetInterface( IE_WAV_CLASS_ID );
	if (soundmgr == NULL) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	if (!soundmgr->Init()) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Allocating SaveGameIterator...", WHITE );
	sgiterator = new SaveGameIterator();
	if (sgiterator == NULL) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Initializing Variables Dictionary...", WHITE );
	vars->SetType( GEM_VARIABLES_INT );
	{
		char ini_path[_MAX_PATH];
		PathJoin( ini_path, GamePath, INIConfig, NULL );
		ResolveFilePath( ini_path );
		LoadINI( ini_path );
		int i;
		for (i = 0; i < 8; i++) {
			if (INIConfig[i] == '.')
				break;
			GameNameResRef[i] = INIConfig[i];
		}
		GameNameResRef[i] = 0;
	}
	//no need of strdup, variables do copy the key!
	vars->SetAt( "SkipIntroVideos", (unsigned long)SkipIntroVideos );
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Initializing Token Dictionary...", WHITE );
	tokens = new Variables();
	if (!tokens) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	tokens->SetType( GEM_VARIABLES_STRING );
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Initializing Music Manager...", WHITE );
	music = ( MusicMgr * ) GetInterface( IE_MUS_CLASS_ID );
	if (!music) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );
	if (HasFeature( GF_HAS_PARTY_INI )) {
		printMessage( "Core", "Loading precreated teams setup...",
			WHITE );
		INIparty = ( DataFileMgr * ) GetInterface( IE_INI_CLASS_ID );
		FileStream* fs = new FileStream();
		char tINIparty[_MAX_PATH];
		PathJoin( tINIparty, GamePath, "Party.ini", NULL );
		ResolveFilePath( tINIparty );
		fs->Open( tINIparty, true );
		if (!INIparty->Open( fs, true )) {
			printStatus( "ERROR", LIGHT_RED );
		} else {
			printStatus( "OK", LIGHT_GREEN );
		}
	}
	if (HasFeature( GF_HAS_BEASTS_INI )) {
		printMessage( "Core", "Loading beasts definition File...",
			WHITE );
		INIbeasts = ( DataFileMgr * ) GetInterface( IE_INI_CLASS_ID );
		FileStream* fs = new FileStream();
		char tINIbeasts[_MAX_PATH];
		PathJoin( tINIbeasts, GamePath, "beast.ini", NULL );
		ResolveFilePath( tINIbeasts );
		// FIXME: crashes if file does not open
		fs->Open( tINIbeasts, true );
		if (!INIbeasts->Open( fs, true )) {
			printStatus( "ERROR", LIGHT_RED );
		} else {
			printStatus( "OK", LIGHT_GREEN );
		}

		printMessage( "Core", "Loading quests definition File...",
			WHITE );
		INIquests = ( DataFileMgr * ) GetInterface( IE_INI_CLASS_ID );
		FileStream* fs2 = new FileStream();
		char tINIquests[_MAX_PATH];
		PathJoin( tINIquests, GamePath, "quests.ini", NULL );
		ResolveFilePath( tINIquests );
		// FIXME: crashes if file does not open
		fs2->Open( tINIquests, true );
		if (!INIquests->Open( fs2, true )) {
			printStatus( "ERROR", LIGHT_RED );
		} else {
			printStatus( "OK", LIGHT_GREEN );
		}
	}
	game = NULL;//new Game();
	printMessage( "Core", "Loading Cursors...", WHITE );

	str = key->GetResource( "CURSORS", IE_BAM_CLASS_ID );
	if (anim->Open( str, true ))
	{
		CursorCount = anim->GetCycleCount();
		Cursors = new Sprite2D * [CursorCount];
		for (int i = 0; i < CursorCount; i++) {
			Cursors[i] = anim->GetFrameFromCycle( i, 0 );
		}
	}

	// this is the last existing cursor type
	if (CursorCount<IE_CURSOR_WAY) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	video->SetCursor( Cursors[0], Cursors[1] );
	printStatus( "OK", LIGHT_GREEN );

	// Load fog-of-war bitmaps
	str = key->GetResource( "FOGOWAR", IE_BAM_CLASS_ID );
	printMessage( "Core", "Loading Fog-Of-War bitmaps...", WHITE );
	anim->Open( str, true );
	if (anim->GetCycleSize( 0 ) != 8) {
		// unknown type of fog anim
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}

	FogSprites[0] = NULL;
	FogSprites[1] = anim->GetFrameFromCycle( 0, 0 );
	FogSprites[2] = anim->GetFrameFromCycle( 0, 1 );
	FogSprites[3] = anim->GetFrameFromCycle( 0, 2 );

	FogSprites[4] = video->MirrorSpriteVertical( FogSprites[1], false );

	FogSprites[5] = NULL;

	FogSprites[6] = video->MirrorSpriteVertical( FogSprites[3], false );

	FogSprites[7] = NULL;

	FogSprites[8] = video->MirrorSpriteHorizontal( FogSprites[2], false );

	FogSprites[9] = video->MirrorSpriteHorizontal( FogSprites[3], false );

	FogSprites[10] = NULL;
	FogSprites[11] = NULL;

	FogSprites[12] = video->MirrorSpriteHorizontal( FogSprites[6], false );

	FogSprites[16] = anim->GetFrameFromCycle( 0, 3 );
	FogSprites[17] = anim->GetFrameFromCycle( 0, 4 );
	FogSprites[18] = anim->GetFrameFromCycle( 0, 5 );
	FogSprites[19] = anim->GetFrameFromCycle( 0, 6 );

	FogSprites[20] = video->MirrorSpriteVertical( FogSprites[17], false );

	FogSprites[21] = NULL;

	FogSprites[23] = NULL;

	FogSprites[24] = video->MirrorSpriteHorizontal( FogSprites[18], false );

	FogSprites[25] = anim->GetFrameFromCycle( 0, 7 );

	{
	Sprite2D *tmpsprite = video->MirrorSpriteVertical( FogSprites[25], false );
	FogSprites[22] = video->MirrorSpriteHorizontal( tmpsprite, false );
	video->FreeSprite( tmpsprite );
	}

	FogSprites[26] = NULL;
	FogSprites[27] = NULL;

	{
	Sprite2D *tmpsprite = video->MirrorSpriteVertical( FogSprites[19], false );
	FogSprites[28] = video->MirrorSpriteHorizontal( tmpsprite, false );
	video->FreeSprite( tmpsprite );
	}

	printStatus( "OK", LIGHT_GREEN );

	{
		ieDword i = 0;
		vars->Lookup("Translucent Shadows", i);
		if (i) {
			for(i=0;i<sizeof(FogSprites)/sizeof(Sprite2D *);i++ ) {
				video->CreateAlpha( FogSprites[i] );
			}
		}
	}

	printMessage( "Core", "Bringing up the Global Timer...", WHITE );
	timer = new GlobalTimer();
	if (!timer) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Initializing effects...", WHITE );
	if (! Init_EffectQueue()) {
		printStatus( "ERROR", LIGHT_RED );
		goto end_of_init;
	}
	printStatus( "OK", LIGHT_GREEN );

	printMessage( "Core", "Initializing Inventory Management...", WHITE );
	ret = InitItemTypes();
	if (ret) {
		printStatus( "OK", LIGHT_GREEN );
	}
	else {
		printStatus( "ERROR", LIGHT_RED );
	}

	printMessage( "Core", "Initializing Spellbook Management...", WHITE );
	ret = Spellbook::InitializeSpellbook();
	if (ret) {
		printStatus( "OK", LIGHT_GREEN );
	}
	else {
		printStatus( "ERROR", LIGHT_RED );
	}

	printMessage( "Core", "Initializing string constants...", WHITE );
	ret = ReadStrrefs();
	if (ret) {
		printStatus( "OK", LIGHT_GREEN );
	}
	else {
		printStatus( "ERROR", LIGHT_RED );
	}

	printMessage( "Core", "Initializing random treasure...", WHITE );
	ret = ReadRandomItems();
	if (ret) {
		printStatus( "OK", LIGHT_GREEN );
	}
	else {
		printStatus( "ERROR", LIGHT_RED );
	}
	printMessage( "Core", "Core Initialization Complete!\n", WHITE );
	ret = GEM_OK;
end_of_init:
	FreeInterface( anim );
	return ret;
}

bool Interface::IsAvailable(SClass_ID filetype)
{
	return plugin->IsAvailable( filetype );
}

WorldMap *Interface::GetWorldMap(const char *map)
{
	unsigned int index = worldmap->FindAndSetCurrentMap(map?map:game->CurrentArea);
	return worldmap->GetWorldMap(index);
}

void* Interface::GetInterface(SClass_ID filetype)
{
	return plugin->GetPlugin( filetype );
}

Video* Interface::GetVideoDriver() const
{
	return video;
}

ResourceMgr* Interface::GetResourceMgr() const
{
	return key;
}

const char* Interface::TypeExt(SClass_ID type)
{
	switch (type) {
		case IE_2DA_CLASS_ID:
			return ".2DA";

		case IE_ACM_CLASS_ID:
			return ".ACM";

		case IE_ARE_CLASS_ID:
			return ".ARE";

		case IE_BAM_CLASS_ID:
			return ".BAM";

		case IE_BCS_CLASS_ID:
			return ".BCS";

		case IE_BIF_CLASS_ID:
			return ".BIF";

		case IE_BMP_CLASS_ID:
			return ".BMP";

		case IE_CHR_CLASS_ID:
			return ".CHR";

		case IE_CHU_CLASS_ID:
			return ".CHU";

		case IE_CRE_CLASS_ID:
			return ".CRE";

		case IE_DLG_CLASS_ID:
			return ".DLG";

		case IE_EFF_CLASS_ID:
			return ".EFF";

		case IE_GAM_CLASS_ID:
			return ".GAM";

		case IE_IDS_CLASS_ID:
			return ".IDS";

		case IE_INI_CLASS_ID:
			return ".INI";

		case IE_ITM_CLASS_ID:
			return ".ITM";

		case IE_KEY_CLASS_ID:
			return ".KEY";

		case IE_MOS_CLASS_ID:
			return ".MOS";

		case IE_MUS_CLASS_ID:
			return ".MUS";

		case IE_MVE_CLASS_ID:
			return ".MVE";

		case IE_PLT_CLASS_ID:
			return ".PLT";

		case IE_PRO_CLASS_ID:
			return ".PRO";

		case IE_SAV_CLASS_ID:
			return ".SAV";

		case IE_SPL_CLASS_ID:
			return ".SPL";

		case IE_SRC_CLASS_ID:
			return ".SRC";

		case IE_STO_CLASS_ID:
			return ".STO";

		case IE_TIS_CLASS_ID:
			return ".TIS";

		case IE_TLK_CLASS_ID:
			return ".TLK";

		case IE_TOH_CLASS_ID:
			return ".TOH";

		case IE_TOT_CLASS_ID:
			return ".TOT";

		case IE_VAR_CLASS_ID:
			return ".VAR";

		case IE_VVC_CLASS_ID:
			return ".VVC";

		case IE_WAV_CLASS_ID:
			return ".WAV";

		case IE_WED_CLASS_ID:
			return ".WED";

		case IE_WFX_CLASS_ID:
			return ".WFX";

		case IE_WMP_CLASS_ID:
			return ".WMP";
	}
	return NULL;
}

char* Interface::GetString(ieStrRef strref, unsigned long options)
{
	ieDword flags = 0;

	if (!(options & IE_STR_STRREFOFF)) {
		vars->Lookup( "Strref On", flags );
	}
	return strings->GetString( strref, flags | options );
}

void Interface::FreeInterface(void* ptr)
{
	plugin->FreePlugin( ptr );
}

Factory* Interface::GetFactory(void) const
{
	return factory;
}

int Interface::SetFeature(int flag, int position)
{
	if (flag) {
		GameFeatures |= 1 << position;
	} else {
		GameFeatures &= ~( 1 << position );
	}
	return GameFeatures;
}
int Interface::HasFeature(int position) const
{
	return GameFeatures & ( 1 << position );
}

/** Search directories and load a config file */
bool Interface::LoadConfig(void)
{
#ifndef WIN32
	char path[_MAX_PATH];
	char name[_MAX_PATH];

	// Find directory where user stores GemRB configurations (~/.gemrb).
	// FIXME: Create it if it does not exist
	// Use current dir if $HOME is not defined (or bomb out??)

	char* s = getenv( "HOME" );
	if (s) {
		strcpy( UserDir, s );
		strcat( UserDir, "/."PACKAGE"/" );
	} else {
		strcpy( UserDir, "./" );
	}

	// Find basename of this program. It does the same as basename (3),
	// but that's probably missing on some archs 
	s = strrchr( argv[0], PathDelimiter );
	if (s) {
		s++;
	} else {
		s = argv[0];
	}

	strcpy( name, s );
	//if (!name[0])		// FIXME: could this happen?
	//	strcpy (name, PACKAGE); // ugly hack

	// If we were called as $0 -c <filename>, load config from filename
	if (argc > 2 && ! strcmp("-c", argv[1])) {
		if (LoadConfig( argv[2] )) {
			return true;
		} else {
			// Explicitly specified cfg file HAS to be present
			return false;
		}
	}

	// FIXME: temporary hack, to be deleted??
	if (LoadConfig( "GemRB.cfg" )) {
		return true;
	}

	PathJoin( path, UserDir, name, NULL );
	strcat( path, ".cfg" );

	if (LoadConfig( path )) {
		return true;
	}

#ifdef SYSCONFDIR
	PathJoin( path, SYSCONFDIR, name, NULL );
	strcat( path, ".cfg" );

	if (LoadConfig( path )) {
		return true;
	}
#endif

	// Don't try with default binary name if we have tried it already
	if (!strcmp( name, PACKAGE )) {
		return false;
	}

	PathJoin( path, UserDir, PACKAGE, NULL );
	strcat( path, ".cfg" );

	if (LoadConfig( path )) {
		return true;
	}

#ifdef SYSCONFDIR
	PathJoin( path, SYSCONFDIR, PACKAGE, NULL );
	strcat( path, ".cfg" );

	if (LoadConfig( path )) {
		return true;
	}
#endif

	return false;
#else // WIN32
	strcpy( UserDir, ".\\" );
	return LoadConfig( "GemRB.cfg" );
#endif// WIN32
}

bool Interface::LoadConfig(const char* filename)
{
	FILE* config;
	config = fopen( filename, "rb" );
	if (config == NULL) {
		return false;
	}
	char name[65], value[_MAX_PATH + 3];

	SaveAsOriginal = 0;

	while (!feof( config )) {
		char rem;
		fread( &rem, 1, 1, config );
		if (rem == '#') {
			fscanf( config, "%*[^\r\n]%*[\r\n]" );
			continue;
		}
		fseek( config, -1, SEEK_CUR );
		fscanf( config, "%64[^=]=%[^\r\n]%*[\r\n]", name, value );
		if (stricmp( name, "Width" ) == 0) {
			Width = atoi( value );
		} else if (stricmp( name, "Height" ) == 0) {
			Height = atoi( value );
		} else if (stricmp( name, "Bpp" ) == 0) {
			Bpp = atoi( value );
		} else if (stricmp( name, "FullScreen" ) == 0) {
			FullScreen = ( atoi( value ) == 0 ) ? false : true;
		} else if (stricmp( name, "SkipIntroVideos" ) == 0) {
			SkipIntroVideos = ( atoi( value ) == 0 ) ? false : true;
		} else if (stricmp( name, "DrawFPS" ) == 0) {
			DrawFPS = ( atoi( value ) == 0 ) ? false : true;
		} else if (stricmp( name, "EnableCheatKeys" ) == 0) {
			EnableCheatKeys ( atoi( value ) );
		} else if (stricmp( name, "FogOfWar" ) == 0) {
			FogOfWar = atoi( value );
		} else if (stricmp( name, "EndianSwitch" ) == 0) {
			DataStream::SetEndianSwitch(atoi(value) );
		} else if (stricmp( name, "CaseSensitive" ) == 0) {
			CaseSensitive = ( atoi( value ) == 0 ) ? false : true;
		} else if (stricmp( name, "VolumeAmbients" ) == 0) {
			vars->SetAt( "Volume Ambients", atoi( value ) );
		} else if (stricmp( name, "VolumeMovie" ) == 0) {
			vars->SetAt( "Volume Movie", atoi( value ) );
		} else if (stricmp( name, "VolumeMusic" ) == 0) {
			vars->SetAt( "Volume Music", atoi( value ) );
		} else if (stricmp( name, "VolumeSFX" ) == 0) {
			vars->SetAt( "Volume SFX", atoi( value ) );
		} else if (stricmp( name, "VolumeVoices" ) == 0) {
			vars->SetAt( "Volume Voices", atoi( value ) );
		} else if (stricmp( name, "GameOnCD" ) == 0) {
			GameOnCD = ( atoi( value ) == 0 ) ? false : true;
		} else if (stricmp( name, "TooltipDelay" ) == 0) {
			TooltipDelay = atoi( value );
		} else if (stricmp( name, "GameDataPath" ) == 0) {
			strncpy( GameData, value, sizeof(GameData) );
		} else if (stricmp( name, "GameOverridePath" ) == 0) {
			strncpy( GameOverride, value, sizeof(GameOverride) );
		} else if (stricmp( name, "GameScriptsPath" ) == 0) {
			strncpy( GameScripts, value, sizeof(GameScripts) );
		} else if (stricmp( name, "GameSoundsPath" ) == 0) {
			strncpy( GameSounds, value, sizeof(GameSounds) );
		} else if (stricmp( name, "GameName" ) == 0) {
			strncpy( GameName, value, sizeof(GameName) );
		} else if (stricmp( name, "GameType" ) == 0) {
			strncpy( GameType, value, sizeof(GameType) );
		} else if (stricmp( name, "SaveAsOriginal") == 0) {
			SaveAsOriginal = atoi(value);
		} else if (stricmp( name, "GemRBPath" ) == 0) {
			strcpy( GemRBPath, value );
		} else if (stricmp( name, "ScriptDebugMode" ) == 0) {
			SetScriptDebugMode(atoi(value));
		} else if (stricmp( name, "CachePath" ) == 0) {
			strncpy( CachePath, value, sizeof(CachePath) );
			FixPath( CachePath, false );
			mkdir( CachePath, S_IREAD|S_IWRITE|S_IEXEC );
			chmod( CachePath, S_IREAD|S_IWRITE|S_IEXEC );
			if (! dir_exists( CachePath )) {
				printf( "Cache folder %s doesn't exist!", CachePath );
				fclose( config );
				return false;
			}
			DelTree((const char *) CachePath, false);
		} else if (stricmp( name, "GUIScriptsPath" ) == 0) {
			strncpy( GUIScriptsPath, value, sizeof(GUIScriptsPath) );
#ifndef WIN32
			ResolveFilePath( GUIScriptsPath );
#endif
		} else if (stricmp( name, "PluginsPath" ) == 0) {
			strncpy( PluginsPath, value, sizeof(PluginsPath) );
#ifndef WIN32
			ResolveFilePath( PluginsPath );
#endif
		} else if (stricmp( name, "GamePath" ) == 0) {
			strncpy( GamePath, value, sizeof(GamePath) );
#ifndef WIN32
			ResolveFilePath( GamePath );
#endif
		} else if (stricmp( name, "SavePath" ) == 0) {
			strncpy( SavePath, value, sizeof(SavePath) );
#ifndef WIN32
			ResolveFilePath( SavePath );
#endif
		} else if (stricmp( name, "CD1" ) == 0) {
			strncpy( CD1, value, sizeof(CD1) );
#ifndef WIN32
			ResolveFilePath( CD1 );
#endif
		} else if (stricmp( name, "CD2" ) == 0) {
			strncpy( CD2, value, sizeof(CD2) );
#ifndef WIN32
			ResolveFilePath( CD2 );
#endif
		} else if (stricmp( name, "CD3" ) == 0) {
			strncpy( CD3, value, sizeof(CD3) );
#ifndef WIN32
			ResolveFilePath( CD3 );
#endif
		} else if (stricmp( name, "CD4" ) == 0) {
			strncpy( CD4, value, sizeof(CD4) );
#ifndef WIN32
			ResolveFilePath( CD4 );
#endif
		} else if (stricmp( name, "CD5" ) == 0) {
			strncpy( CD5, value, sizeof(CD5) );
#ifndef WIN32
			ResolveFilePath( CD5 );
#endif
		} else if (stricmp( name, "CD6" ) == 0) {
			strncpy( CD6, value, sizeof(CD6) );
#ifndef WIN32
			ResolveFilePath( CD6 );
#endif
		}
	}
	fclose( config );

	if (!GameType[0]) {
		strcpy( GameType, "gemrb");
	}

#ifdef DATADIR
	if (!GemRBPath[0]) {
		strcpy( GemRBPath, DATADIR );
		strcat( GemRBPath, SPathDelimiter );
	}
#endif
	if (!PluginsPath[0]) {
#ifdef PLUGINDIR
		strcpy( PluginsPath, PLUGINDIR );
#else
		PathJoin( PluginsPath, GemRBPath, "plugins", NULL );
#endif
		strcat( PluginsPath, SPathDelimiter );
	}
	FixPath(GemRBPath, true);
	FixPath(CachePath, true);
	if (GUIScriptsPath[0]) {
		FixPath(GUIScriptsPath, true);
	}
	else {
		memcpy( GUIScriptsPath, GemRBPath, sizeof( GUIScriptsPath ) );
	}
	if (!GameName[0]) {
		strcpy( GameName, GEMRB_STRING );
	}
	if (!SavePath[0]) {
		// FIXME: maybe should use UserDir instead of GamePath
		memcpy( SavePath, GamePath, sizeof( GamePath ) );
	}

	printf( "Loaded config file %s\n", filename );
	return true;
}

static void upperlower(int upper, int lower)
{
	pl_uppercase[lower]=upper;
	pl_lowercase[upper]=lower;
}

/** Loads gemrb.ini */
bool Interface::LoadGemRBINI()
{
	DataStream* inifile = key->GetResource( "gemrb", IE_INI_CLASS_ID );
	if (! inifile) {
		printStatus( "ERROR", LIGHT_RED );
		return false;
	}

	printMessage( "Core", "\nLoading game type-specific GemRB setup...", WHITE );

	if (!IsAvailable( IE_INI_CLASS_ID )) {
		printStatus( "ERROR", LIGHT_RED );
		printf( "[Core]: No INI Importer Available.\n" );
		return false;
	}
	DataFileMgr* ini = ( DataFileMgr* ) GetInterface( IE_INI_CLASS_ID );
	ini->Open( inifile, true ); //autofree

	printStatus( "OK", LIGHT_GREEN );

	const char *s;

	// Resrefs are already initialized in Interface::Interface() 
	s = ini->GetKeyAsString( "resources", "CursorBAM", NULL );
	if (s)
		strcpy( CursorBam, s );

	s = ini->GetKeyAsString( "resources", "ButtonFont", NULL );
	if (s)
		strcpy( ButtonFont, s );

	s = ini->GetKeyAsString( "resources", "TooltipFont", NULL );
	if (s)
		strcpy( TooltipFont, s );

	s = ini->GetKeyAsString( "resources", "TooltipBack", NULL );
	if (s)
		strcpy( TooltipBackResRef, s );

	s = ini->GetKeyAsString( "resources", "TooltipColor", NULL );
	if (s) {
		if (s[0] == '#') {
			unsigned long c = strtoul (s + 1, NULL, 16);
			// FIXME: check errno
			TooltipColor.r = (unsigned char) (c >> 24);
			TooltipColor.g = (unsigned char) (c >> 16);
			TooltipColor.b = (unsigned char) (c >> 8);
			TooltipColor.a = (unsigned char) (c);
		}
	}

	TooltipMargin = ini->GetKeyAsInt( "resources", "TooltipMargin", TooltipMargin );

	s = ini->GetKeyAsString( "resources", "INIConfig", NULL );
	if (s)
		strcpy( INIConfig, s );
	
	s = ini->GetKeyAsString( "resources", "Palette16", NULL );
	if (s)
		strcpy( Palette16, s );

	s = ini->GetKeyAsString( "resources", "Palette32", NULL );
	if (s)
		strcpy( Palette32, s );

	s = ini->GetKeyAsString( "resources", "Palette256", NULL );
	if (s)
		strcpy( Palette256, s );

	unsigned int i;
	for(i=0;i<256;i++) {
		pl_uppercase[i]=toupper(i);
		pl_lowercase[i]=tolower(i);
	}

	i = (unsigned int) ini->GetKeyAsInt ("charset", "CharCount", 0);
	if (i>99) i=99;
	while(i--) {
		char key[10];
		snprintf(key,9,"Letter%d", i+1);
		s = ini->GetKeyAsString( "charset", key, NULL );
		if (s) {
			char *s2 = strchr(s,',');
			if (s2) {
				upperlower(atoi(s), atoi(s2+1) );
				printMessage("Core"," ",WHITE);
				printf("Upperlower %d %d ",atoi(s), atoi(s2+1) );
				printStatus( "SET", LIGHT_GREEN );
			}
		}
	}

	SetFeature( ini->GetKeyAsInt( "resources", "IWD2ScriptName", 0 ), GF_IWD2_SCRIPTNAME );
	SetFeature( ini->GetKeyAsInt( "resources", "HasSpellList", 0 ), GF_HAS_SPELLLIST );
	SetFeature( ini->GetKeyAsInt( "resources", "ProtagonistTalks", 0 ), GF_PROTAGONIST_TALKS );
	SetFeature( ini->GetKeyAsInt( "resources", "AutomapIni", 0 ), GF_AUTOMAP_INI );
	SetFeature( ini->GetKeyAsInt( "resources", "IWDMapDimensions", 0 ), GF_IWD_MAP_DIMENSIONS );
	SetFeature( ini->GetKeyAsInt( "resources", "OneByteAnimationID", 0 ), GF_ONE_BYTE_ANIMID );
	SetFeature( ini->GetKeyAsInt( "resources", "IgnoreButtonFrames", 1 ), GF_IGNORE_BUTTON_FRAMES );
	SetFeature( ini->GetKeyAsInt( "resources", "AllStringsTagged", 1 ), GF_ALL_STRINGS_TAGGED );
	SetFeature( ini->GetKeyAsInt( "resources", "HasDPLAYER", 0 ), GF_HAS_DPLAYER );
	SetFeature( ini->GetKeyAsInt( "resources", "HasPickSound", 0 ), GF_HAS_PICK_SOUND );
	SetFeature( ini->GetKeyAsInt( "resources", "HasDescIcon", 0 ), GF_HAS_DESC_ICON );
	SetFeature( ini->GetKeyAsInt( "resources", "HasEXPTABLE", 0 ), GF_HAS_EXPTABLE );
	SetFeature( ini->GetKeyAsInt( "resources", "HasKaputz", 0 ), GF_HAS_KAPUTZ );
	SetFeature( ini->GetKeyAsInt( "resources", "SoundFolders", 0 ), GF_SOUNDFOLDERS );
	SetFeature( ini->GetKeyAsInt( "resources", "HasSongList", 0 ), GF_HAS_SONGLIST );
	SetFeature( ini->GetKeyAsInt( "resources", "UpperButtonText", 0 ), GF_UPPER_BUTTON_TEXT );
	SetFeature( ini->GetKeyAsInt( "resources", "LowerLabelText", 0 ), GF_LOWER_LABEL_TEXT );
	SetFeature( ini->GetKeyAsInt( "resources", "HasPartyIni", 0 ), GF_HAS_PARTY_INI );
	SetFeature( ini->GetKeyAsInt( "resources", "HasBeastsIni", 0 ), GF_HAS_BEASTS_INI );
	SetFeature( ini->GetKeyAsInt( "resources", "TeamMovement", 0 ), GF_TEAM_MOVEMENT );
	SetFeature( ini->GetKeyAsInt( "resources", "SmallFog", 1 ), GF_SMALL_FOG );
	SetFeature( ini->GetKeyAsInt( "resources", "ReverseDoor", 0 ), GF_REVERSE_DOOR );
	ForceStereo = ini->GetKeyAsInt( "resources", "ForceStereo", 0 );

	FreeInterface( ini );
	return true;
}

/** No descriptions */
Color* Interface::GetPalette(int index, int colors)
{
	Color* pal = NULL;
	if (colors == 32) {
		pal = ( Color * ) malloc( colors * sizeof( Color ) );
		pal32->GetPalette( index, colors, pal );
	} else if (colors <= 32) {
		pal = ( Color * ) malloc( colors * sizeof( Color ) );
		pal16->GetPalette( index, colors, pal );
	} else if (colors == 256) {
		pal = ( Color * ) malloc( colors * sizeof( Color ) );
		pal256->GetPalette( index, colors, pal );
	}
	return pal;
}
/** Returns a preloaded Font */
Font* Interface::GetFont(const char *ResRef) const
{
	for (unsigned int i = 0; i < fonts.size(); i++) {
		if (strnicmp( fonts[i]->ResRef, ResRef, 8 ) == 0) {
			return fonts[i];
		}
	}
	return NULL;
}

Font* Interface::GetFont(unsigned int index) const
{
	if (index >= fonts.size()) {
		return NULL;
	}
	return fonts[index];
}

Font* Interface::GetButtonFont() const
{
	return GetFont( ButtonFont );
}

/** Returns the Event Manager */
EventMgr* Interface::GetEventMgr() const
{
	return evntmgr;
}

/** Returns the Window Manager */
WindowMgr* Interface::GetWindowMgr() const
{
	return windowmgr;
}

/** Get GUI Script Manager */
ScriptEngine* Interface::GetGUIScriptEngine() const
{
	return guiscript;
}

Actor *Interface::GetCreature(DataStream *stream)
{
	ActorMgr* actormgr = ( ActorMgr* ) GetInterface( IE_CRE_CLASS_ID );
	if (!actormgr->Open( stream, true )) {
		FreeInterface( actormgr );
		return NULL;
	}
	Actor* actor = actormgr->GetActor();
	FreeInterface( actormgr );
	return actor;
}

int Interface::LoadCreature(char* ResRef, int InParty, bool character)
{
	DataStream *stream;

	if (character) {
		char nPath[_MAX_PATH], fName[16];
		snprintf( fName, sizeof(fName), "%s.chr", ResRef);
		PathJoin( nPath, GamePath, "characters", fName, NULL );
#ifndef WIN32
		ResolveFilePath( nPath );
#endif
		FileStream *fs = new FileStream();
		fs -> Open( nPath, true );
		stream = (DataStream *) fs;
	}
	else {
		stream = key->GetResource( ResRef, IE_CRE_CLASS_ID );
	}
	Actor* actor = GetCreature(stream);
	if ( !actor ) {
		return -1;
	}
	actor->InParty = InParty;
	//both fields are of length 9, make this sure!
	memcpy(actor->Area, game->CurrentArea, sizeof(actor->Area) );
	if (actor->BaseStats[IE_STATE_ID] & STATE_DEAD) {
		actor->SetStance( IE_ANI_TWITCH );
	} else {
		actor->SetStance( IE_ANI_AWAKE );
	}
	actor->SetOrientation( 0, 0 );

	if ( InParty ) {
		return game->JoinParty( actor, JP_JOIN|JP_INITPOS );
	}
	else {
		return game->AddNPC( actor );
	}
}

int Interface::GetCreatureStat(unsigned int Slot, unsigned int StatID, int Mod)
{
	Actor * actor = game->FindPC(Slot);
	if (!actor) {
		return 0xdadadada;
	}

	if (Mod) {
		return actor->GetStat( StatID );
	}
	return actor->GetBase( StatID );
}

int Interface::SetCreatureStat(unsigned int Slot, unsigned int StatID,
	int StatValue, int Mod)
{
	Actor * actor = game->FindPC(Slot);
	if (!actor) {
		return 0;
	}
	if (Mod) {
		actor->SetStat( StatID, StatValue );
	} else {
		actor->SetBase( StatID, StatValue );
	}
	return 1;
}

void Interface::RedrawControls(char *varname, unsigned int value)
{
	for (unsigned int i = 0; i < windows.size(); i++) {
		if (windows[i] != NULL) {
			windows[i]->RedrawControls(varname, value);
		}
	}
}

void Interface::RedrawAll()
{
	for (unsigned int i = 0; i < windows.size(); i++) {
		if (windows[i] != NULL) {
			windows[i]->Invalidate();
		}
	}
}

/** Loads a WindowPack (CHUI file) in the Window Manager */
bool Interface::LoadWindowPack(const char* name)
{
	DataStream* stream = key->GetResource( name, IE_CHU_CLASS_ID );
	if (stream == NULL) {
		printMessage( "Interface", "Error: Cannot find ", LIGHT_RED );
		printf( "%s.chu\n", name );
		return false;
	}
	if (!GetWindowMgr()->Open( stream, true )) {
		printMessage( "Interface", "Error: Cannot Load ", LIGHT_RED );
		printf( "%s.chu\n", name );
		return false;
	}

	strncpy( WindowPack, name, sizeof( WindowPack ) );
	WindowPack[sizeof( WindowPack ) - 1] = '\0';

	return true;
}

/** Loads a Window in the Window Manager */
int Interface::LoadWindow(unsigned short WindowID)
{
	unsigned int i;

	for (i = 0; i < windows.size(); i++) {
		Window *win = windows[i];
		if (win == NULL)
			continue;
		if (win->Visible==-1) {
			continue;
		}
		if (win->WindowID == WindowID && 
			!strnicmp( WindowPack, win->WindowPack, sizeof(WindowPack) )) {
			SetOnTop( i );
			win->Invalidate();
			return i;
		}
	}
	Window* win = windowmgr->GetWindow( WindowID );
	if (win == NULL) {
		return -1;
	}
	memcpy( win->WindowPack, WindowPack, sizeof(WindowPack) );

	int slot = -1;
	for (i = 0; i < windows.size(); i++) {
		if (windows[i] == NULL) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		windows.push_back( win );
		slot = ( int ) windows.size() - 1;
	} else {
		windows[slot] = win;
	}
	win->Invalidate();
	return slot;
}
// FIXME: it's a clone of LoadWindow
/** Creates a Window in the Window Manager */
int Interface::CreateWindow(unsigned short WindowID, int XPos, int YPos, unsigned int Width, unsigned int Height, char* Background)
{
	unsigned int i;

	for (i = 0; i < windows.size(); i++) {
		if (windows[i] == NULL)
			continue;
		if (windows[i]->WindowID == WindowID && !stricmp( WindowPack,
													windows[i]->WindowPack )) {
			SetOnTop( i );
			windows[i]->Invalidate();
			return i;
		}
	}

	Window* win = new Window( WindowID, XPos, YPos, Width, Height );
	if (Background[0]) {
		if (IsAvailable( IE_MOS_CLASS_ID )) {
			DataStream* bkgr = key->GetResource( Background,
														IE_MOS_CLASS_ID );
			if (bkgr != NULL) {
				ImageMgr* mos = ( ImageMgr* )
					GetInterface( IE_MOS_CLASS_ID );
				mos->Open( bkgr, true );
				win->SetBackGround( mos->GetImage(), true );
				FreeInterface( mos );
			} else
				printf( "[Core]: Cannot Load BackGround, skipping\n" );
		} else
			printf( "[Core]: No MOS Importer Available, skipping background\n" );
	}

	strcpy( win->WindowPack, WindowPack );

	int slot = -1;
	for (i = 0; i < windows.size(); i++) {
		if (windows[i] == NULL) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		windows.push_back( win );
		slot = ( int ) windows.size() - 1;
	} else {
		windows[slot] = win;
	}
	win->Invalidate();
	return slot;
}

/** Sets a Window on the Top */
void Interface::SetOnTop(int Index)
{
	std::vector<int>::iterator t;
	for(t = topwin.begin(); t != topwin.end(); ++t) {
		if((*t) == Index) {
			topwin.erase(t);
			break;
		}
	}
	if(topwin.size() != 0)
		topwin.insert(topwin.begin(), Index);
	else
		topwin.push_back(Index);
}
/** Add a window to the Window List */
void Interface::AddWindow(Window * win)
{
	int slot = -1;
	for(unsigned int i = 0; i < windows.size(); i++) {
		Window *w = windows[i];
		
		if(w==NULL) {
			slot = i;
			break;
		}
	}
	if(slot == -1) {
		windows.push_back(win);
		slot=(int)windows.size()-1;
	}
	else
		windows[slot] = win;
	win->Invalidate();
}

/** Get a Control on a Window */
int Interface::GetControl(unsigned short WindowIndex, unsigned long ControlID)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	int i = 0;
	while (true) {
		Control* ctrl = win->GetControl( i );
		if (ctrl == NULL)
			return -1;
		if (ctrl->ControlID == ControlID)
			return i;
		i++;
	}
}
/** Adjust the Scrolling factor of a control (worldmap atm) */
int Interface::AdjustScrolling(unsigned short WindowIndex,
	unsigned short ControlIndex, short x, short y)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	Control* ctrl = win->GetControl( ControlIndex );
	if (ctrl == NULL) {
		return -1;
	}
	switch(ctrl->ControlType) {
		case IE_GUI_WORLDMAP:
			((WorldMapControl *) ctrl)->AdjustScrolling(x,y);
			break;
		default: //doesn't work for these
			return -1;
	}
	return 0;
}

/** Set the Text of a Control */
int Interface::SetText(unsigned short WindowIndex,
	unsigned short ControlIndex, const char* string)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	Control* ctrl = win->GetControl( ControlIndex );
	if (ctrl == NULL) {
		return -1;
	}
	return ctrl->SetText( string );
}
/** Set the Tooltip text of a Control */
int Interface::SetTooltip(unsigned short WindowIndex,
	unsigned short ControlIndex, const char* string)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	Control* ctrl = win->GetControl( ControlIndex );
	if (ctrl == NULL) {
		return -1;
	}
	return ctrl->SetTooltip( string );
}

void Interface::DisplayTooltip(int x, int y, Control *ctrl)
{
	tooltip_x = x;
	tooltip_y = y;
	tooltip_ctrl = ctrl;
}

int Interface::GetVisible(unsigned short WindowIndex)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	return win->Visible;
}
/** Set a Window Visible Flag */
int Interface::SetVisible(unsigned short WindowIndex, int visible)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	win->Visible = visible;
	switch (visible) {
		case WINDOW_GRAYED:
			win->Invalidate();
		case WINDOW_INVISIBLE:
			//hiding the viewport if the gamecontrol window was made invisible
			if (win->WindowID==65535) {
				video->SetViewport( 0,0,0,0 );
			}
			//evntmgr->DelWindow( win->WindowID, win->WindowPack );
			evntmgr->DelWindow( win );
			break;

		case WINDOW_VISIBLE:
			if (win->WindowID==65535) {
				video->SetViewport( win->XPos, win->YPos, win->Width, win->Height);
			}
			evntmgr->AddWindow( win );
			win->Invalidate();
			SetOnTop( WindowIndex );
			break;
	}
	return 0;
}


/** Set the Status of a Control in a Window */
int Interface::SetControlStatus(unsigned short WindowIndex,
	unsigned short ControlIndex, unsigned long Status)
{
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		return -1;
	}
	Control* ctrl = win->GetControl( ControlIndex );
	if (ctrl == NULL) {
		return -1;
	}
	if (Status&IE_GUI_CONTROL_FOCUSED) {
			evntmgr->SetFocused( win, ctrl);
	}
	if (ctrl->ControlType != ((Status >> 24) & 0xff) ) {
		return -2;
	}
	switch (ctrl->ControlType) {
		case IE_GUI_BUTTON:
		//Button
		 {
			Button* btn = ( Button* ) ctrl;
			btn->SetState( ( unsigned char ) ( Status & 0x7f ) );
		}
		break;
		default:
			ctrl->Value = Status & 0x7f;
			break;
	}
	return 0;
}

/** Show a Window in Modal Mode */
int Interface::ShowModal(unsigned short WindowIndex, int Shadow)
{
	if (WindowIndex >= windows.size()) {
		printMessage( "Core", "Window not found", LIGHT_RED );
		return -1;
	}
	Window* win = windows[WindowIndex];
	if (win == NULL) {
		printMessage( "Core", "Window already freed", LIGHT_RED );
		return -1;
	}
	win->Visible = WINDOW_VISIBLE;
	evntmgr->Clear();
	SetOnTop( WindowIndex );
	evntmgr->AddWindow( win );

	ModalWindow = NULL;
	DrawWindows();
	win->Invalidate();

	Color gray = {
		0, 0, 0, 128
	};
	Color black = {
		0, 0, 0, 255
	};

	Region r( 0, 0, Width, Height );

	if (Shadow == MODAL_SHADOW_GRAY) {
		video->DrawRect( r, gray );
	} else if (Shadow == MODAL_SHADOW_BLACK) {
		video->DrawRect( r, black );
	}

	ModalWindow = win;
	return 0;
}

void Interface::DrawWindows(void)
{
	GameControl *gc = GetGameControl();
	if (!gc) {
		GSUpdate(false);
	}
	else {
		//this variable is used all over in the following hacks
		int flg = gc->GetDialogueFlags();
		GSUpdate(!(flg & DF_FREEZE_SCRIPTS) );

		//the following part is a series of hardcoded gui behaviour

		//updating panes according to the saved game
		//pst requires this before initiating dialogs because it has
		//no dialog window by default
		ieDword index = 0;

		if (!vars->Lookup( "MessageWindowSize", index ) || (index!=game->ControlStatus) ) {
			vars->SetAt( "MessageWindowSize", game->ControlStatus);
			guiscript->RunFunction( "UpdateControlStatus" );
			//giving control back to GameControl
			SetControlStatus(0,0,0x7f000000|IE_GUI_CONTROL_FOCUSED);
			GameControl *gc = GetGameControl();
			//this is the only value we can use here
			if (game->ControlStatus & CS_HIDEGUI)
				gc->HideGUI();
			else
				gc->UnhideGUI();
		}

		//initiating dialog
		if (flg & DF_IN_DIALOG) {
			// -3 noaction
			// -2 close
			// -1 open
			// choose option
			ieDword var = (ieDword) -3;
			vars->Lookup("DialogChoose", var);
			if ((int) var == -2) {
				gc->EndDialog();
			} else if ( (int)var !=-3) {
				gc->DialogChoose(var);
				vars->SetAt("DialogChoose", (ieDword) -3);
			}
			if (flg & DF_OPENCONTINUEWINDOW) {
				guiscript->RunFunction( "OpenContinueMessageWindow" );
				gc->SetDialogueFlags(DF_OPENCONTINUEWINDOW|DF_OPENENDWINDOW, BM_NAND);
			} else if (flg & DF_OPENENDWINDOW) {
				guiscript->RunFunction( "OpenEndMessageWindow" );
				gc->SetDialogueFlags(DF_OPENCONTINUEWINDOW|DF_OPENENDWINDOW, BM_NAND);
			}
		}

		//handling container
		if (CurrentContainer && UseContainer) {
			if (!(flg & DF_IN_CONTAINER) ) {
				gc->SetDialogueFlags(DF_IN_CONTAINER, BM_OR);
				guiscript->RunFunction( "OpenContainerWindow" );
			}
		} else {
			if (flg & DF_IN_CONTAINER) {
				gc->SetDialogueFlags(DF_IN_CONTAINER, BM_NAND);
				guiscript->RunFunction( "CloseContainerWindow" );
			}
		}
		//end of gui hacks
	}
	
	//here comes the REAL drawing of windows
	if (ModalWindow) {
		ModalWindow->DrawWindow();
		return;
	}
	std::vector< int>::reverse_iterator t = topwin.rbegin();
	size_t i = topwin.size();
	while(i--) {
		if ( (unsigned int) ( *t ) >=windows.size() )
			continue;
		//visible ==1 or 2 will be drawn
		Window* win = windows[(*t)];
		if (win != NULL) {
			if (win->Visible == -1) {
				topwin.erase(topwin.begin()+i);
				//evntmgr->DelWindow( win->WindowID, win->WindowPack );
				evntmgr->DelWindow( win );
				delete win;
				windows[(*t)]=NULL;
			}
			else if (win->Visible) {
				windows[( *t )]->DrawWindow();
			}
		}
		++t;
	}
}

void Interface::DrawTooltip ()
{	
	if (! tooltip_ctrl || !tooltip_ctrl->Tooltip) 
		return;

	Font* fnt = GetFont( TooltipFont );
	char *tooltip_text = tooltip_ctrl->Tooltip;

	int w1 = 0;
	int w2 = 0;
	int w = fnt->CalcStringWidth( tooltip_text );
	int h = fnt->maxHeight;

	if (TooltipBack) {
		h = TooltipBack[0]->Height;
		w1 = TooltipBack[1]->Width;
		w2 = TooltipBack[2]->Width;
		w += TooltipMargin + w1 + w2;
	}


	int x = tooltip_x - w / 2;
	int y = tooltip_y - h / 2;

	// Ensure placement within the screen
	if (x < 0) x = 0;
	else if (x + w > Width) 
		x = Width - w;
	if (y < 0) y = 0;
	else if (y + h > Height) 
		y = Height - h;


	// FIXME: add tooltip scroll animation for bg. also, take back[0] from
	// center, not from left end
	if (TooltipBack) {
		Region r2 = Region( x + w1, y, w - (w1 + w2), h );
		video->BlitSprite( TooltipBack[0], x + w1, y, true, &r2 );
		video->BlitSprite( TooltipBack[1], x, y, true );
		video->BlitSprite( TooltipBack[2], x + w - w2, y, true );
	}

//	Color back = {0x00, 0x00, 0x00, 0x00};
//	Color* palette = video->CreatePalette( TooltipColor, back );
	
	fnt->Print( Region( x, y, w, h ), (ieByte *) tooltip_text, NULL,
		IE_FONT_ALIGN_CENTER | IE_FONT_ALIGN_MIDDLE | IE_FONT_SINGLE_LINE, true );
}

//interface for higher level functions, if the window was
//marked for deletion it is not returned
Window* Interface::GetWindow(unsigned short WindowIndex)
{
	if (WindowIndex < windows.size()) {
		Window *win = windows[WindowIndex];
		if (win && (win->Visible!=-1) ) {
			return win;
		}
	}
	return NULL;
}

//this function won't delete the window, just mark it for deletion
//it will be deleted in the next DrawWindows cycle
//regardless, the window deleted is inaccessible for gui scripts and
//other high level functions from now
int Interface::DelWindow(unsigned short WindowIndex)
{
	if (WindowIndex == 0xffff) {
		//we clear ALL windows immediately, don't call this
		//from a guiscript
		vars->SetAt("MessageWindow", (ieDword) ~0);
		vars->SetAt("OptionsWindow", (ieDword) ~0);
		vars->SetAt("PortraitWindow", (ieDword) ~0);
		vars->SetAt("ActionsWindow", (ieDword) ~0);
		vars->SetAt("TopWindow", (ieDword) ~0);
		vars->SetAt("OtherWindow", (ieDword) ~0);
		vars->SetAt("FloatWindow", (ieDword) ~0);
		for(unsigned int WindowIndex=0; WindowIndex<windows.size();WindowIndex++) {
			Window* win = windows[WindowIndex];
			if (win) {
				delete( win );
			}
		}
		windows.clear();
		topwin.clear();
		evntmgr->Clear();
		ModalWindow = NULL;
		return 0;
	}
	if (WindowIndex >= windows.size()) {
		return -1;
	}
	Window* win = windows[WindowIndex];
	if ((win == NULL) || (win->Visible==-1) ) {
		printMessage( "Core", "Window deleted again", LIGHT_RED );
		return -1;
	}
	if (win == ModalWindow) {
		ModalWindow = NULL;
	}
	//evntmgr->DelWindow( win->WindowID, win->WindowPack );
	evntmgr->DelWindow( win );
//	delete( win );
	win->release();
//	windows[WindowIndex] = NULL;
	/*
	std::vector< int>::iterator t;
	for (t = topwin.begin(); t != topwin.end(); ++t) {
		if (( *t ) == WindowIndex) {
			topwin.erase( t );
			break;
		}
	}
	*/
	return 0;
}

/** Popup the Console */
void Interface::PopupConsole()
{
	ConsolePopped = !ConsolePopped;
	RedrawAll();
	console->Changed = true;
}

/** Draws the Console */
void Interface::DrawConsole()
{
	console->Draw( 0, 0 );
}

/** Get the Sound Manager */
SoundMgr* Interface::GetSoundMgr()
{
	return soundmgr;
}
/** Get the Sound Manager */
SaveGameIterator* Interface::GetSaveGameIterator()
{
	return sgiterator;
}
/** Sends a termination signal to the Video Driver */
bool Interface::Quit(void)
{
	return video->Quit();
}
/** Returns the variables dictionary */
Variables* Interface::GetDictionary()
{
	return vars;
}
/** Returns the token dictionary */
Variables* Interface::GetTokenDictionary()
{
	return tokens;
}
/** Get the Music Manager */
MusicMgr* Interface::GetMusicMgr()
{
	return music;
}
/** Loads a 2DA Table, returns -1 on error or the Table Index on success */
int Interface::LoadTable(const ieResRef ResRef)
{
	int ind = GetTableIndex( ResRef );
	if (ind != -1) {
		return ind;
	}
	//printf("(%s) Table not found... Loading from file\n", ResRef);
	DataStream* str = key->GetResource( ResRef, IE_2DA_CLASS_ID );
	if (!str) {
		return -1;
	}
	TableMgr* tm = ( TableMgr* ) GetInterface( IE_2DA_CLASS_ID );
	if (!tm) {
		delete( str );
		return -1;
	}
	if (!tm->Open( str, true )) {
		FreeInterface( tm );
		return -1;
	}
	Table t;
	t.free = false;
	strncpy( t.ResRef, ResRef, 8 );
	t.tm = tm;
	ind = -1;
	for (size_t i = 0; i < tables.size(); i++) {
		if (tables[i].free) {
			ind = ( int ) i;
			break;
		}
	}
	if (ind != -1) {
		tables[ind] = t;
		return ind;
	}
	tables.push_back( t );
	return ( int ) tables.size() - 1;
}
/** Gets the index of a loaded table, returns -1 on error */
int Interface::GetTableIndex(const char* ResRef)
{
	for (size_t i = 0; i < tables.size(); i++) {
		if (tables[i].free)
			continue;
		if (strnicmp( tables[i].ResRef, ResRef, 8 ) == 0)
			return ( int ) i;
	}
	return -1;
}
/** Gets a Loaded Table by its index, returns NULL on error */
TableMgr* Interface::GetTable(unsigned int index)
{
	if (index >= tables.size()) {
		return NULL;
	}
	if (tables[index].free) {
		return NULL;
	}
	return tables[index].tm;
}
/** Frees a Loaded Table, returns false on error, true on success */
bool Interface::DelTable(unsigned int index)
{
	if (index==0xffffffff) {
		FreeInterfaceVector( Table, tables, tm );
		tables.clear();
		return true;
	}
	if (index >= tables.size()) {
		return false;
	}
	if (tables[index].free) {
		return false;
	}
	FreeInterface( tables[index].tm );
	tables[index].free = true;
	return true;
}
/** Loads an IDS Table, returns -1 on error or the Symbol Table Index on success */
int Interface::LoadSymbol(const char* ResRef)
{
	int ind = GetSymbolIndex( ResRef );
	if (ind != -1) {
		return ind;
	}
	DataStream* str = key->GetResource( ResRef, IE_IDS_CLASS_ID );
	if (!str) {
		return -1;
	}
	SymbolMgr* sm = ( SymbolMgr* ) GetInterface( IE_IDS_CLASS_ID );
	if (!sm) {
		delete( str );
		return -1;
	}
	if (!sm->Open( str, true )) {
		FreeInterface( sm );
		return -1;
	}
	Symbol s;
	s.free = false;
	strncpy( s.ResRef, ResRef, 8 );
	s.sm = sm;
	ind = -1;
	for (size_t i = 0; i < symbols.size(); i++) {
		if (symbols[i].free) {
			ind = ( int ) i;
			break;
		}
	}
	if (ind != -1) {
		symbols[ind] = s;
		return ind;
	}
	symbols.push_back( s );
	return ( int ) symbols.size() - 1;
}
/** Gets the index of a loaded Symbol Table, returns -1 on error */
int Interface::GetSymbolIndex(const char* ResRef)
{
	for (size_t i = 0; i < symbols.size(); i++) {
		if (symbols[i].free)
			continue;
		if (strnicmp( symbols[i].ResRef, ResRef, 8 ) == 0)
			return ( int ) i;
	}
	return -1;
}
/** Gets a Loaded Symbol Table by its index, returns NULL on error */
SymbolMgr* Interface::GetSymbol(unsigned int index)
{
	if (index >= symbols.size()) {
		return NULL;
	}
	if (symbols[index].free) {
		return NULL;
	}
	return symbols[index].sm;
}
/** Frees a Loaded Symbol Table, returns false on error, true on success */
bool Interface::DelSymbol(unsigned int index)
{
	if (index >= symbols.size()) {
		return false;
	}
	if (symbols[index].free) {
		return false;
	}
	FreeInterface( symbols[index].sm );
	symbols[index].free = true;
	return true;
}
/** Plays a Movie */
int Interface::PlayMovie(char* ResRef)
{
	MoviePlayer* mp = ( MoviePlayer* ) GetInterface( IE_MVE_CLASS_ID );
	if (!mp) {
		return 0;
	}
	DataStream* str = key->GetResource( ResRef, IE_MVE_CLASS_ID );
	if (!str) {
		FreeInterface( mp );
		return -1;
	}
	if (!mp->Open( str, true )) {
		FreeInterface( mp );
	// since mp was opened with autofree, this delete would cause double free
	//	delete( str );
		return -1;
	}
	// Disable all the windows, so they do not receive mouse clicks
	// FIXME: of course, movies played before GameControl is created
	// are different story
	GameControl* gc = GetGameControl();
	if (gc) gc->HideGUI();
	else SetVisible (0, WINDOW_INVISIBLE);

	//shutting down music and ambients before movie
	if (music) music->HardEnd();
	soundmgr->GetAmbientMgr()->deactivate();
	mp->Play();
	//restarting music
	if (music) music->Start();
	soundmgr->GetAmbientMgr()->activate();
	if (gc) gc->UnhideGUI();
	else SetVisible (0, WINDOW_VISIBLE);

	FreeInterface( mp );
	//Setting the movie name to 1
	vars->SetAt( ResRef, 1 );
	return 0;
}

int Interface::Roll(int dice, int size, int add)
{
	if (dice < 1) {
		return add;
	}
	if (size < 1) {
		return add;
	}
	if (dice > 100) {
		return add + dice * size / 2;
	}
	for (int i = 0; i < dice; i++) {
		add += rand() % size + 1;
	}
	return add;
}

bool Interface::SavingThrow(int Save, int Bonus)
{
	int roll = Roll(1, 20, 0);
	// FIXME: this is 2e saving throw, it's probably different in iwd2
	return (roll > 1) && (roll + Bonus >= Save);
}

int Interface::GetCharSounds(TextArea* ta)
{
	bool hasfolders;
	int count = 0;
	char Path[_MAX_PATH];

	PathJoin( Path, GamePath, GameSounds, NULL );
	hasfolders = ( HasFeature( GF_SOUNDFOLDERS ) != 0 );
	DIR* dir = opendir( Path );
	if (dir == NULL) {
		return -1;
	}
	//Lookup the first entry in the Directory
	struct dirent* de = readdir( dir );
	if (de == NULL) {
		closedir( dir );
		return -1;
	}
	printf( "Looking in %s\n", Path );
	do {
		if (de->d_name[0] == '.')
			continue;
		char dtmp[_MAX_PATH];
		PathJoin( dtmp, Path, de->d_name, NULL );
		struct stat fst;
		stat( dtmp, &fst );
		if (hasfolders == !S_ISDIR( fst.st_mode ))
			continue;
		count++;
		ta->AppendText( de->d_name, -1 );
	} while (( de = readdir( dir ) ) != NULL);
	closedir( dir );
	return count;
}

bool Interface::LoadINI(const char* filename)
{
	FILE* config;
	config = fopen( filename, "rb" );
	if (config == NULL) {
		return false;
	}
	char name[65], value[_MAX_PATH + 3];
	while (!feof( config )) {
		name[0] = 0;
		value[0] = 0;
		char rem;
		fread( &rem, 1, 1, config );
		if (( rem == '#' ) ||
			( rem == '[' ) ||
			( rem == '\r' ) ||
			( rem == '\n' ) ||
			( rem == ';' )) {
			if (rem == '\r') {
				fgetc( config );
				continue;
			} else if (rem == '\n')
				continue;
			fscanf( config, "%*[^\r\n]%*[\r\n]" );
			continue;
		}
		fseek( config, -1, SEEK_CUR );
		fscanf( config, "%[^=]=%[^\r\n]%*[\r\n]", name, value );
		if (( value[0] >= '0' ) && ( value[0] <= '9' )) {
			vars->SetAt( name, atoi( value ) );
		}
	}
	fclose( config );
	return true;
}

/** Enables/Disables the Cut Scene Mode */
void Interface::SetCutSceneMode(bool active)
{
/*
	if (!active) {
		timer->SetCutScene( NULL );
	}
*/
	GameControl *gc = GetGameControl();
	if (gc) {
		gc->SetCutSceneMode( active );
	}
	if (game) {
		if (active) {
			game->ControlStatus |= CS_HIDEGUI;
		} else {
			game->ControlStatus &= ~CS_HIDEGUI;
		}
	}
	video->DisableMouse = active;
	video->moveX = 0;
	video->moveY = 0;
}

bool Interface::InCutSceneMode()
{
	return (GetGameControl()->GetScreenFlags()&SF_DISABLEMOUSE)!=0;
}

void Interface::QuitGame(bool BackToMain)
{
	//clear cutscenes
	SetCutSceneMode(false);
	//clear fade/screenshake effects
	timer->Init();
	timer->SetFadeFromColor(0);

	DelWindow(0xffff); //delete all windows, including GameControl

	//delete game, worldmap
	if (game) {
		delete game;
		game=NULL;
	}
	if (worldmap) {
		delete worldmap;
		worldmap=NULL;
	}
	//shutting down ingame music
	if (music) music->HardEnd();
	// stop any ambients which are still enqueued
	soundmgr->GetAmbientMgr()->deactivate(); 
	if (BackToMain) {
		strcpy(NextScript, "Start");
		QuitFlag |= QF_CHANGESCRIPT;
	}
	GSUpdate(true);
}

void Interface::LoadGame(int index)
{
	// This function has rather painful error handling,
	// as it should swap all the objects or none at all
	// and the loading can fail for various reasons

	// Yes, it uses goto. Other ways seemed too awkward for me.

	tokens->RemoveAll(); //clearing the token dictionary
	DataStream* gam_str = NULL;
	DataStream* sav_str = NULL;
	DataStream* wmp_str = NULL;

	SaveGameMgr* gam_mgr = NULL;
	WorldMapMgr* wmp_mgr = NULL;

	Game* new_game = NULL;
	WorldMapArray* new_worldmap = NULL;

	LoadProgress(0);
	DelTree((const char *) CachePath, true);
	LoadProgress(5);

	if (index == -1) {
		//Load the Default Game
		gam_str = key->GetResource( GameNameResRef, IE_GAM_CLASS_ID );
		sav_str = NULL;
		wmp_str = key->GetResource( WorldMapName, IE_WMP_CLASS_ID );
	} else {
		SaveGame* sg = sgiterator->GetSaveGame( index );
		if (!sg)
			return;
		gam_str = sg->GetGame();
		sav_str = sg->GetSave();
		wmp_str = sg->GetWmap();
		delete sg;
	}

	if (!gam_str || !wmp_str)
		goto cleanup;


	// Load GAM file
	gam_mgr = ( SaveGameMgr* ) GetInterface( IE_GAM_CLASS_ID );
	if (!gam_mgr)
		goto cleanup;

	if (!gam_mgr->Open( gam_str, true ))
		goto cleanup;

	new_game = gam_mgr->GetGame();
	if (!new_game)
		goto cleanup;

	FreeInterface( gam_mgr );
	gam_mgr = NULL;
	gam_str = NULL;


	// Load WMP (WorldMap) file
	wmp_mgr = ( WorldMapMgr* ) GetInterface( IE_WMP_CLASS_ID );
	if (! wmp_mgr)
		goto cleanup;
	
	if (!wmp_mgr->Open( wmp_str, true ))
		goto cleanup;

	new_worldmap = wmp_mgr->GetWorldMapArray( );

	FreeInterface( wmp_mgr );
	wmp_mgr = NULL;
	wmp_str = NULL;

	LoadProgress(10);
	// Unpack SAV (archive) file to Cache dir
	if (sav_str) {
		ArchiveImporter * ai = (ArchiveImporter*)GetInterface(IE_BIF_CLASS_ID);
		if (ai) {
			ai->DecompressSaveGame(sav_str);
			FreeInterface( ai );
			ai = NULL;
		}
		delete( sav_str );
		sav_str = NULL;
	}

	// Let's assume that now is everything loaded OK and swap the objects

	if (game)
		delete( game );

	if (worldmap)
		delete( worldmap );

	game = new_game;
	worldmap = new_worldmap;

	LoadProgress(100);
	return;
 cleanup:
	// Something went wrong, so try to clean after itself
	if (new_game)
		delete( new_game );
	if (new_worldmap)
		delete( new_worldmap );

	if (gam_mgr) {
		FreeInterface( gam_mgr );
		gam_str = NULL;
	}
	if (wmp_mgr) {
		FreeInterface( wmp_mgr );
		wmp_str = NULL;
	}

	if (gam_str) delete gam_str;
	if (wmp_str) delete wmp_str;
	if (sav_str) delete sav_str;
}

GameControl *Interface::GetGameControl()
{
 	Window *window = GetWindow( 0 );
	// in the beginning, there's no window at all
	if (! window)
		return NULL;

	Control* gc = window->GetControl(0);
	if (gc->ControlType!=IE_GUI_GAMECONTROL) {
		return NULL;
	}
	return (GameControl *) gc;
}

bool Interface::InitItemTypes()
{
	if (slotmatrix) {
		free(slotmatrix);
	}
	int ItemTypeTable = LoadTable( "itemtype" );
	TableMgr *it = GetTable(ItemTypeTable);

	ItemTypes = 0;
	if (it) {
		ItemTypes = it->GetRowCount(); //number of itemtypes
		if (ItemTypes<0) {
			ItemTypes = 0;
		}
		int InvSlotTypes = it->GetColumnCount();
		if (InvSlotTypes > 32) { //bit count limit
			InvSlotTypes = 32;
		}
		//make sure unsigned int is 32 bits
		slotmatrix = (ieDword *) malloc(ItemTypes * sizeof(ieDword) );
		for (int i=0;i<ItemTypes;i++) {
			unsigned int value = 0;
			unsigned int k = 1;
			for (int j=0;j<InvSlotTypes;j++) {
				if (strtol(it->QueryField(i,j),NULL,0) ) {
					value |= k;
				}
				k <<= 1;
			}
			slotmatrix[i] = (ieDword) value;
		}
		DelTable(ItemTypeTable);
	}

	//slottype describes the inventory structure
	int SlotTypeTable = LoadTable( "slottype" );
	TableMgr *st = GetTable(SlotTypeTable);
	if (slottypes) {
		free(slottypes);
	}
	SlotTypes = 0;
	if (st) {
		SlotTypes = st->GetRowCount();
		//make sure unsigned int is 32 bits
		slottypes = (SlotType *) malloc(SlotTypes * sizeof(SlotType) );
		for (int i = 0; i < SlotTypes; i++) {
			slottypes[i].slottype = (ieDword) strtol(st->QueryField(i,0),NULL,0 );
			slottypes[i].slotid = (ieDword) strtol(st->QueryField(i,1),NULL,0 );
			slottypes[i].slottip = (ieDword) strtol(st->QueryField(i,3),NULL,0 );
			slottypes[i].sloteffects = (ieDword) strtol(st->QueryField(i,4),NULL,0 );
			//make a macro for this?
			/*
			strncpy(slottypes[i].slotresref, st->QueryField(i,2), 8 );
			slottypes[i].slotresref[8]=0;
			strupr(slottypes[i].slotresref);
			*/
			strnuprcpy( slottypes[i].slotresref, st->QueryField(i,2), 8 );
		}
		DelTable( SlotTypeTable );
	}
	return (it && st);
}

int Interface::QuerySlotType(int idx) const
{
	if (idx>=SlotTypes) {
		return 0;
	}
	return slottypes[idx].slottype;
}

int Interface::QuerySlotID(int idx) const
{
	if (idx>=SlotTypes) {
		return 0;
	}
	return slottypes[idx].slotid;
}

int Interface::QuerySlottip(int idx) const
{
	if (idx>=SlotTypes) {
		return 0;
	}
	return slottypes[idx].slottip;
}

int Interface::QuerySlotEffects(int idx) const
{
	if (idx>=SlotTypes) {
		return 0;
	}
	return slottypes[idx].sloteffects;
}

const char *Interface::QuerySlotResRef(int idx) const
{
	if (idx>=SlotTypes) {
		return "";
	}
	return slottypes[idx].slotresref;
}

int Interface::CanUseItemType(int itype, int slottype) const
{
	if ( !slottype ) { 
		//inventory slot, can hold any item, including invalid
		return 1;
	}
	if ( itype>=ItemTypes ) {
		//invalid itemtype
		return 0;
	}
	//if any bit is true, we return true (int->bool conversion)
	return (slotmatrix[itype]&slottype);
}

TextArea *Interface::GetMessageTextArea()
{
	ieDword WinIndex, TAIndex;

	vars->Lookup( "MessageWindow", WinIndex );
	if (( WinIndex != (ieDword) -1 ) &&
		( vars->Lookup( "MessageTextArea", TAIndex ) )) {
		Window* win = GetWindow( (unsigned short) WinIndex );
		if (win) {
			Control *ctrl = win->GetControl( (unsigned short) TAIndex );
			if (ctrl->ControlType==IE_GUI_TEXTAREA)
				return (TextArea *) ctrl;
		}
	}
	return NULL;
}

void Interface::DisplayString(const char* Text)
{
	TextArea *ta = GetMessageTextArea();
	if (ta)
		ta->AppendText( Text, -1 );
}

static const char* DisplayFormatName = "[color=%lX]%s - [/color][p][color=%lX]%s[/color][/p]";
static const char* DisplayFormat = "[/color][p][color=%lX]%s[/color][/p]";
static const char* DisplayFormatValue = "[/color][p][color=%lX]%s: %d[/color][/p]";

void Interface::DisplayConstantString(int stridx, unsigned int color)
{
	char* text = GetString( strref_table[stridx], IE_STR_SOUND );
	int newlen = (int)(strlen( DisplayFormat ) + strlen( text ) + 12);
	char* newstr = ( char* ) malloc( newlen );
	snprintf( newstr, newlen, DisplayFormat, color, text );
	free( text );
	DisplayString( newstr );
	free( newstr );
}

void Interface::DisplayConstantStringValue(int stridx, unsigned int color, ieDword value)
{
	char* text = GetString( strref_table[stridx], IE_STR_SOUND );
	int newlen = (int)(strlen( DisplayFormat ) + strlen( text ) + 28);
	char* newstr = ( char* ) malloc( newlen );
	snprintf( newstr, newlen, DisplayFormatValue, color, text, (int) value );
	free( text );
	DisplayString( newstr );
	free( newstr );
}

void Interface::DisplayConstantStringName(int stridx, unsigned int color, Scriptable *speaker)
{
	unsigned int speaker_color;
	char *name;
	Color *tmp;

	switch (speaker->Type) {
		case ST_ACTOR:
			name = ((Actor *) speaker)->GetName(-1);
			tmp = GetPalette( ((Actor *) speaker)->GetStat(IE_MAJOR_COLOR),1 );
			speaker_color = (tmp[0].r<<16) | (tmp[0].g<<8) | tmp[0].b;
			free(tmp);
			break;
		default:
			name = "";
			speaker_color = 0x800000;
			break;
	}

	char* text = GetString( strref_table[stridx], IE_STR_SOUND|IE_STR_SPEECH );
	int newlen = (int)(strlen( DisplayFormatName ) + strlen( name ) +
		+ strlen( text ) + 18);
	char* newstr = ( char* ) malloc( newlen );
	sprintf( newstr, DisplayFormatName, speaker_color, name, color,
		text );
	free( text );
	DisplayString( newstr );
	free( newstr );
}

void Interface::DisplayStringName(int stridx, unsigned int color, Scriptable *speaker)
{
	unsigned int speaker_color;
	char *name;
	Color *tmp;

	switch (speaker->Type) {
		case ST_ACTOR:
			name = ((Actor *) speaker)->GetName(-1);
			tmp = GetPalette( ((Actor *) speaker)->GetStat(IE_MAJOR_COLOR),1 );
			speaker_color = (tmp[0].r<<16) | (tmp[0].g<<8) | tmp[0].b;
			free(tmp);
			break;
		default:
			name = "";
			speaker_color = 0x800000;
			break;
	}

	char* text = GetString( stridx, IE_STR_SOUND|IE_STR_SPEECH );
	int newlen = (int)(strlen( DisplayFormatName ) + strlen( name ) +
		+ strlen( text ) + 10);
	char* newstr = ( char* ) malloc( newlen );
	sprintf( newstr, DisplayFormatName, speaker_color, name, color,
		text );
	free( text );
	DisplayString( newstr );
	free( newstr );
}

static char *saved_extensions[]={".are",".sto",".tot",".toh",0};

//returns true if file should be saved
bool Interface::SavedExtension(const char *filename)
{
	char *str=strchr(filename,'.');
	if (!str) return false;
	int i=0;
	while(saved_extensions[i]) {
		if (!stricmp(saved_extensions[i], str) ) return true;
		i++;
	}
	return false;
}

void Interface::RemoveFromCache(const ieResRef resref)
{
	char filename[_MAX_PATH];

	strcpy( filename, CachePath );
	strcat( filename, resref );
	unlink ( filename);
}

void Interface::DelTree(const char* Pt, bool onlysave)
{
	char Path[_MAX_PATH];
	strcpy( Path, Pt );
	DIR* dir = opendir( Path );
	if (dir == NULL) {
		return;
	}
	struct dirent* de = readdir( dir ); //Lookup the first entry in the Directory
	if (de == NULL) {
		closedir( dir );
		return;
	}
	do {
		char dtmp[_MAX_PATH];
		struct stat fst;
		snprintf( dtmp, _MAX_PATH, "%s%s%s", Path, SPathDelimiter, de->d_name );
		stat( dtmp, &fst );
		if (S_ISDIR( fst.st_mode ))
			continue;
		if (de->d_name[0] == '.')
			continue;
		if (!onlysave || SavedExtension(de->d_name) ) {
			unlink( dtmp );
		}
	} while (( de = readdir( dir ) ) != NULL);
	closedir( dir );
}

void Interface::LoadProgress(int percent)
{
	vars->SetAt("Progress", percent);
	RedrawControls("Progress", percent);
	RedrawAll();
	DrawWindows();
	video->SwapBuffers();
}

void Interface::DragItem(CREItem *item)
{
	// FIXME: what if we already drag st.?
	// Avenger: We should drop the dragged item and pick this up
	// We shouldn't have a valid DraggedItem at this point
	DraggedItem = item;
}

bool Interface::ReadItemTable(const ieResRef TableName, const char * Prefix)
{
	ieResRef ItemName;
	TableMgr * tab;
	ieResRef *itemlist;
	int i,j;

	int table=LoadTable(TableName);
	if (table<0) {
		return false;
	}
	tab = GetTable(table);
	if (!tab) {
		goto end;
	}
	i=tab->GetRowCount();
	for(j=0;j<i;j++) {
		if (Prefix) {
			snprintf(ItemName,sizeof(ItemName),"%s%02d",Prefix, j+1);
		} else {
			strncpy(ItemName,tab->GetRowName(j),sizeof(ieResRef) );
		}
		//Variable elements are free'd, so we have to use malloc
		int l=tab->GetColumnCount(j);
		if (l<1) continue;
		//we just allocate one more ieResRef for the item count
		itemlist = (ieResRef *) malloc( sizeof(ieResRef) * (l+1) );
		//ieResRef (9 bytes) is bigger than int (on any platform)
		*(int *) itemlist=l;
		for(int k=1;k<=l;k++) {
			strncpy(itemlist[k],tab->QueryField(j,k),sizeof(ieResRef) );
		}
		ItemName[8]=0;
		strupr(ItemName);
		RtRows->SetAt(ItemName, (const char *) itemlist);
	}
end:
	DelTable(table);
	return true;
}

bool Interface::ReadRandomItems()
{
	ieResRef RtResRef;
	int i;
	TableMgr * tab;

	int table=LoadTable( "randitem" );
	int difflev=0; //rt norm or rt fury

	if (RtRows) {
		RtRows->RemoveAll();
	}
	else {
		RtRows=new Variables(10, 17); //block size, hash table size
		if (!RtRows) {
			return false;
		}
		RtRows->SetType( GEM_VARIABLES_STRING );
	}
	if (table<0) {
		return false;
	}
	tab = GetTable( table );
	if (!tab) {
		goto end;
	}
	strncpy( GoldResRef, tab->QueryField((unsigned int) 0,(unsigned int) 0), sizeof(ieResRef) ); //gold
	if ( GoldResRef[0]=='*' ) {
		DelTable( table );
		return false;
	}
	strncpy( RtResRef, tab->QueryField( 1, difflev ), sizeof(ieResRef) );
	i=atoi( RtResRef );
	if (i<1) {
		ReadItemTable( RtResRef, 0 ); //reading the table itself
		goto end;
	}
	if (i>5) {
		i=5;
	}
	while(i--) {
		strncpy( RtResRef,tab->QueryField(2+i,difflev), sizeof(ieResRef) );
		ReadItemTable( RtResRef,tab->GetRowName(2+i) );
	}
end:
	DelTable( table );
	return true;
}

CREItem *Interface::ReadItem(DataStream *str)
{
	CREItem *itm = new CREItem();

	str->ReadResRef( itm->ItemResRef );
	str->ReadWord( &itm->PurchasedAmount );
	str->ReadWord( &itm->Usages[0] );
	str->ReadWord( &itm->Usages[1] );
	str->ReadWord( &itm->Usages[2] );
	str->ReadDword( &itm->Flags );
	if (ResolveRandomItem(itm) )
	{
		return itm;
	}
	delete itm;
	return NULL;
}

#define MAX_LOOP 10

//This function generates random items based on the randitem.2da file
//there could be a loop, but we don't want to freeze, so there is a limit
bool Interface::ResolveRandomItem(CREItem *itm)
{
	if (!RtRows) return true;
	for(int loop=0;loop<MAX_LOOP;loop++)
	{
		int i,j,k;
		char *endptr;
		ieResRef NewItem;

		char *itemlist=NULL;
		if ( (!RtRows->Lookup( itm->ItemResRef, itemlist )) )
		{
			return true;
		}
		i=Roll(1,*(int *) itemlist,0);
		strncpy( NewItem, ((ieResRef *) itemlist)[i], sizeof(ieResRef) );
		char *p=(char *) strchr(NewItem,'*');
		if (p)
		{
			*p=0; //doing this so endptr is ok
			k=strtol(p+1,NULL,10);
		}
		else {
			k=1;
		}
		j=strtol(NewItem,&endptr,10);
		if (*endptr) strnuprcpy(itm->ItemResRef,NewItem,sizeof(ieResRef) );
		else {
			strnuprcpy(itm->ItemResRef, GoldResRef, sizeof(ieResRef) );
			itm->Usages[0]=Roll(j,k,0);
		}
		if ( !memcmp( itm->ItemResRef,"NO_DROP",8 ) ) {
			itm->ItemResRef[0]=0;
		}
		if (!itm->ItemResRef[0]) return false;
	}
	printf("Loop detected while generating random item:%s",itm->ItemResRef);
	printStatus("ERROR", LIGHT_RED);
	return false;
}

Item* Interface::GetItem(const ieResRef resname)
{
	Item *item = (Item *) ItemCache.GetResource(resname);
	if (item) {
		return item;
	}
	DataStream* str = key->GetResource( resname, IE_ITM_CLASS_ID );
	ItemMgr* sm = ( ItemMgr* ) GetInterface( IE_ITM_CLASS_ID );
	if (sm == NULL) {
		delete ( str );
		return NULL;
	}
	if (!sm->Open( str, true )) {
		FreeInterface( sm );
		return NULL;
	}

	item = new Item();
	//this is required for storing the 'source'
	strnuprcpy(item->Name, resname, 8);
	sm->GetItem( item );
	if (item == NULL) {
		FreeInterface( sm );
		return NULL;
	}

	FreeInterface( sm );
	ItemCache.SetAt(resname, (void *) item);
	return item;
}

//you can supply name for faster access
void Interface::FreeItem(Item *itm, const ieResRef name, bool free)
{
	int res;

	res=ItemCache.DecRef((void *) itm, name, free);
	if (res<0) {
		printMessage( "Core", "Corrupted Item cache encountered (reference count went below zero), ", LIGHT_RED );
		printf( "Item name is: %.8s\n", name);
		abort();
	}
	if (res) return;
	if (free) delete itm;
}

Spell* Interface::GetSpell(const ieResRef resname)
{
	Spell *spell = (Spell *) SpellCache.GetResource(resname);
	if (spell) {
		return spell;
	}
	DataStream* str = key->GetResource( resname, IE_SPL_CLASS_ID );
	SpellMgr* sm = ( SpellMgr* ) GetInterface( IE_SPL_CLASS_ID );
	if (sm == NULL) {
		delete ( str );
		return NULL;
	}
	if (!sm->Open( str, true )) {
		FreeInterface( sm );
		return NULL;
	}

	spell = new Spell();
	//this is required for storing the 'source'
	strnuprcpy(spell->Name, resname, 8);
	sm->GetSpell( spell );
	if (spell == NULL) {
		FreeInterface( sm );
		return NULL;
	}

	FreeInterface( sm );

	SpellCache.SetAt(resname, (void *) spell);
	return spell;
}

void Interface::FreeSpell(Spell *spl, const ieResRef name, bool free)
{
	int res;

	res=SpellCache.DecRef((void *) spl, name, free);
	if (res<0) {
		printMessage( "Core", "Corrupted Spell cache encountered (reference count went below zero), ", LIGHT_RED );
		printf( "Spell name is: %.8s or %.8s\n", name, spl->Name);
		abort();
	}
	if (res) return;
	if (free) delete spl;
}

Effect* Interface::GetEffect(const ieResRef resname)
{
	Effect *effect = (Effect *) EffectCache.GetResource(resname);
	if (effect) {
		return effect;
	}
	DataStream* str = key->GetResource( resname, IE_EFF_CLASS_ID );
	EffectMgr* em = ( EffectMgr* ) GetInterface( IE_EFF_CLASS_ID );
	if (em == NULL) {
		delete ( str );
		return NULL;
	}
	if (!em->Open( str, true )) {
		FreeInterface( em );
		return NULL;
	}

	effect = em->GetEffect(new Effect() );
	if (effect == NULL) {
		FreeInterface( em );
		return NULL;
	}

	FreeInterface( em );

	EffectCache.SetAt(resname, (void *) effect);
	return effect;
}

void Interface::FreeEffect(Effect *eff, const ieResRef name, bool free)
{
	int res;

	res=EffectCache.DecRef((void *) eff, name, free);
	if (res<0) {
		printMessage( "Core", "Corrupted Effect cache encountered (reference count went below zero), ", LIGHT_RED );
		printf( "Effect name is: %.8s\n", name);
		abort();
	}
	if (res) return;
	if (free) delete eff;
}

//now that we store spell name in spl, i guess, we shouldn't pass 'ieResRef name'
//these functions are needed because Win32 doesn't allow freeing memory from
//another dll. So we allocate all commonly used memories from core
ITMExtHeader *Interface::GetITMExt(int count)
{
	return new ITMExtHeader[count];
}

SPLExtHeader *Interface::GetSPLExt(int count)
{
	return new SPLExtHeader[count];
}

Effect *Interface::GetFeatures(int count)
{
	return new Effect[count];
}

void Interface::FreeITMExt(ITMExtHeader *p, Effect *e)
{
	delete [] p;
	delete [] e;
}

void Interface::FreeSPLExt(SPLExtHeader *p, Effect *e)
{
	delete [] p;
	delete [] e;
}

WorldMapArray *Interface::NewWorldMapArray(int count)
{
	return new WorldMapArray(count);
}

Container *Interface::GetCurrentContainer()
{
	return CurrentContainer;
}

int Interface::CloseCurrentContainer()
{
	UseContainer = false;
	if ( !CurrentContainer) {
		return -1;
	}
	//remove empty ground piles on closeup
	CurrentContainer->GetCurrentArea()->TMap->CleanupContainer(CurrentContainer);
	CurrentContainer = NULL;
	return 0;
}

void Interface::SetCurrentContainer(Actor *actor, Container *arg, bool flag)
{
	//abort action if the first selected PC isn't the original actor
	if (actor!=GetFirstSelectedPC()) {
		CurrentContainer = NULL;
		return;
	}
	CurrentContainer = arg;
	UseContainer = flag;
}

Store *Interface::GetCurrentStore()
{
	return CurrentStore;
}

int Interface::CloseCurrentStore()
{
	if ( !CurrentStore ) {
		return -1;
	}
	StoreMgr* sm = ( StoreMgr* ) GetInterface( IE_STO_CLASS_ID );
	if (sm == NULL) {
		return -1;
	}
	int size = sm->GetStoredFileSize (CurrentStore);
	if (size > 0) {
	 	//created streams are always autofree (close file on destruct)
		//this one will be destructed when we return from here
		FileStream str;

		str.Create( CurrentStore->Name, IE_STO_CLASS_ID );
		int ret = sm->PutStore (&str, CurrentStore);
		if (ret <0) {
			printMessage("Core"," ", YELLOW);
			printf("Store removed: %s\n", CurrentStore->Name);
			RemoveFromCache(CurrentStore->Name);
		}
	} else {
		printMessage("Core"," ", YELLOW);
		printf("Store removed: %s\n", CurrentStore->Name);
		RemoveFromCache(CurrentStore->Name);
	}
	//make sure the stream isn't connected to sm, or it will be double freed
	FreeInterface( sm );
	delete CurrentStore;
	CurrentStore = NULL;
	return 0;
}

Store *Interface::SetCurrentStore(const ieResRef resname )
{
	if ( CurrentStore ) {
		if ( !strnicmp(CurrentStore->Name, resname, 8) ) {
			return CurrentStore;
		}

		//not simply delete the old store, but save it
		CloseCurrentStore();
	}

	DataStream* str = key->GetResource( resname, IE_STO_CLASS_ID );
	StoreMgr* sm = ( StoreMgr* ) GetInterface( IE_STO_CLASS_ID );
	if (sm == NULL) {
		delete ( str );
		return NULL;
	}
	if (!sm->Open( str, true )) {
		FreeInterface( sm );
		return NULL;
	}

	// FIXME - should use some already allocated in core
	// not really, only one store is open at a time, then it is
	// unloaded, we don't really have to cache it, it will be saved in
	// Cache anyway!
	CurrentStore = sm->GetStore( new Store() );
	if (CurrentStore == NULL) {
		FreeInterface( sm );
		return NULL;
	}
	FreeInterface( sm );
	strnuprcpy(CurrentStore->Name, resname, 8);

	return CurrentStore;
}

ieStrRef Interface::GetRumour(const ieResRef dlgref)
{
	DialogMgr* dm = ( DialogMgr* ) GetInterface( IE_DLG_CLASS_ID );
	dm->Open( key->GetResource( dlgref, IE_DLG_CLASS_ID ), true );
	Dialog *dlg = dm->GetDialog();
	FreeInterface( dm );

	if (!dlg) {
		printf( "[Interface]: Cannot load dialog: %s\n", dlgref );
		return (ieStrRef) -1;
	}
	Scriptable *pc=game->GetPC( game->GetSelectedPCSingle(), false );
	
	ieStrRef ret = (ieStrRef) -1;
	int i = dlg->FindRandomState( pc );
	if (i>=0 ) {
		ret = dlg->GetState( i )->StrRef;
	}
	delete dlg;
	return ret;
}

void Interface::DoTheStoreHack(Store *s)
{
	size_t size = s->PurchasedCategoriesCount * sizeof( ieDword );
	s->purchased_categories=(ieDword *) malloc(size);

	size = s->CuresCount * sizeof( STOCure );
	s->cures=(STOCure *) malloc(size);

	size = s->DrinksCount * sizeof( STODrink );
	s->drinks=(STODrink *) malloc(size);

	for(size=0;size<s->ItemsCount;size++)
		s->items.push_back( new STOItem() );
}

void Interface::MoveViewportTo(int x, int y, bool center)
{
	video->MoveViewportTo( x, y, center );
	timer->shakeStartVP = video->GetViewport();
}

//plays stock sound listed in defsound.2da
void Interface::PlaySound(int index)
{
	if (index<=DSCount) {
		soundmgr->Play(DefSound[index]);
	}
}

bool Interface::Exists(const char *ResRef, SClass_ID type)
{
	return key->HasResource( ResRef, type );
}

ScriptedAnimation* Interface::GetScriptedAnimation( const char *effect, Point &position )
{
	if (Exists( effect, IE_VVC_CLASS_ID ) ) {
		DataStream *ds = key->GetResource( effect, IE_VVC_CLASS_ID );
		return new ScriptedAnimation( ds, position, true);
	}
	AnimationFactory *af = (AnimationFactory *)
		key->GetFactoryResource( effect, IE_BAM_CLASS_ID, IE_NORMAL );
	return new ScriptedAnimation( af, position);
}

Actor *Interface::GetFirstSelectedPC()
{
	for (int i = 0; i < game->GetPartySize( false ); i++) {
		Actor* actor = game->GetPC( i,false );
		if (actor->IsSelected()) {
			return actor;
		}
	}
	return NULL;
}

// Return single BAM frame as a sprite. Use if you want one frame only,
// otherwise it's not efficient
Sprite2D* Interface::GetBAMSprite(const ieResRef ResRef, int cycle, int frame)
{
	AnimationMgr* bam = ( AnimationMgr* ) GetInterface( IE_BAM_CLASS_ID );
	DataStream *str = key->GetResource( ResRef, IE_BAM_CLASS_ID );
	if (!bam->Open( str, true ) ) {
		return NULL;
	}
	Sprite2D *tspr;
	if (cycle==-1) {
		tspr = bam->GetFrame( frame );
	}
	else {
		tspr = bam->GetFrameFromCycle( (unsigned char) cycle, frame );
	}
	FreeInterface( bam );

	return tspr;
}

Sprite2D *Interface::GetCursorSprite()
{
	return GetBAMSprite(CursorBam, 0, 0);
}

/* we should return -1 if it isn't gold, otherwise return the gold value */
int Interface::CanMoveItem(CREItem *item)
{
	if (item->Flags & IE_INV_ITEM_UNDROPPABLE)
		return 0;
	//not gold, we allow only one single coin ResRef, this is good
	//for all of the original games
	if (strnicmp(item->ItemResRef, GoldResRef, 8 ) )
		return -1;
	//gold, returns the gold value (stack size)
	return item->Usages[0];
}

// dealing with applying effects
void Interface::ApplySpell(const ieResRef resname, Actor *actor, Actor *caster, int level)
{
	Spell *spell = GetSpell(resname);
	if (!spell) {
		return;
	}
	if (!level) {
		level = 1;
	}
	EffectQueue *fxqueue = spell->GetEffectBlock(level);
	fxqueue->SetOwner( caster );
	fxqueue->AddAllEffects(actor);
	delete fxqueue;
}

void Interface::ApplySpellPoint(const ieResRef resname, Scriptable* /*target*/, Point &/*pos*/, Actor *caster, int level)
{
	Spell *spell = GetSpell(resname);
	if (!spell) {
		return;
	}
	if (!level) {
		level = 1;
	}
	EffectQueue *fxqueue = spell->GetEffectBlock(level);
	fxqueue->SetOwner( caster );
	//add effect to area???
	delete fxqueue;
}

void Interface::ApplyEffect(const ieResRef resname, Actor *actor, Actor *caster, int level)
{
	Effect *effect = GetEffect(resname);
	if (!effect) {
		return;
	}
	if (!level) {
		level = 1;
	}
	EffectQueue *fxqueue = new EffectQueue();
	fxqueue->SetOwner( caster );
	fxqueue->AddEffect( effect );
	delete effect;
	fxqueue->AddAllEffects( actor );
	delete fxqueue;
}

// dealing with saved games
int Interface::SwapoutArea(Map *map)
{
	MapMgr* mm = ( MapMgr* ) GetInterface( IE_ARE_CLASS_ID );
	if (mm == NULL) {
		return -1;
	}
	int size = mm->GetStoredFileSize (map);
	if (size > 0) {
	 	//created streams are always autofree (close file on destruct)
		//this one will be destructed when we return from here
		FileStream str;

		str.Create( map->GetScriptName(), IE_ARE_CLASS_ID );
		int ret = mm->PutArea (&str, map);
		if (ret <0) {
			printMessage("Core"," ", YELLOW);
			printf("Area removed: %s\n", map->GetScriptName());
			RemoveFromCache(map->GetScriptName());
		}
	} else {
		printMessage("Core"," ", YELLOW);
		printf("Area removed: %s\n", map->GetScriptName());
		RemoveFromCache(map->GetScriptName());
	}
	//make sure the stream isn't connected to sm, or it will be double freed
	FreeInterface( mm );
	return 0;
}

int Interface::WriteGame(const char *folder)
{
	SaveGameMgr* gm = ( SaveGameMgr* ) GetInterface( IE_GAM_CLASS_ID );
	if (gm == NULL) {
		return -1;
	}

	int size = gm->GetStoredFileSize (game);
	if (size > 0) {
	 	//created streams are always autofree (close file on destruct)
		//this one will be destructed when we return from here
		FileStream str;

		str.Create( folder, GameNameResRef, IE_GAM_CLASS_ID );
		int ret = gm->PutGame (&str, game);
		if (ret <0) {
			printMessage("Core"," ", YELLOW);
			printf("Internal error, game cannot be saved: %s\n", GameNameResRef);
		}
	} else {
		printMessage("Core"," ", YELLOW);
			printf("Internal error, game cannot be saved: %s\n", GameNameResRef);
	}
	//make sure the stream isn't connected to sm, or it will be double freed
	FreeInterface( gm );
	return 0;
}

int Interface::WriteWorldMap(const char *folder)
{
	WorldMapMgr* wmm = ( WorldMapMgr* ) GetInterface( IE_WMP_CLASS_ID );
	if (wmm == NULL) {
		return -1;
	}

	int size = wmm->GetStoredFileSize (worldmap);
	if (size > 0) {
	 	//created streams are always autofree (close file on destruct)
		//this one will be destructed when we return from here
		FileStream str;

		str.Create( folder, WorldMapName, IE_WMP_CLASS_ID );
		int ret = wmm->PutWorldMap (&str, worldmap);
		if (ret <0) {
			printMessage("Core"," ", YELLOW);
			printf("Internal error, worldmap cannot be saved: %s\n", WorldMapName);
		}
	} else {
		printMessage("Core"," ", YELLOW);
		printf("Internal error, worldmap cannot be saved: %s\n", WorldMapName);
	}
	//make sure the stream isn't connected to sm, or it will be double freed
	FreeInterface( wmm );
	return 0;
}

int Interface::CompressSave(const char *folder)
{
	FileStream str;

	str.Create( folder, GameNameResRef, IE_SAV_CLASS_ID );
	DIR* dir = opendir( CachePath );
	if (dir == NULL) {
		return -1;
	}
	struct dirent* de = readdir( dir ); //Lookup the first entry in the Directory
	if (de == NULL) {
		closedir( dir );
		return -1;
	}
	//BIF and SAV are the same
	ArchiveImporter * ai = (ArchiveImporter*)GetInterface(IE_BIF_CLASS_ID);
	ai->CreateArchive( &str);

	do {
		char dtmp[_MAX_PATH];
		struct stat fst;
		snprintf( dtmp, _MAX_PATH, "%s%s", CachePath, de->d_name );
		stat( dtmp, &fst );
		if (S_ISDIR( fst.st_mode ))
			continue;
		if (de->d_name[0] == '.')
			continue;
		if (SavedExtension(de->d_name) ) {
			FileStream fs;
			fs.Open(dtmp, true);
			ai->AddToSaveGame(&str, &fs);
		}
	} while (( de = readdir( dir ) ) != NULL);
	closedir( dir );
	return 0;
}
