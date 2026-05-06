/*
 * cogc_verbs.c -- JK-mode verb/message/global registration for cogc
 *
 * Registers all verbs in the EXACT order the game does at startup, so the
 * verb IDs baked into compiled bytecode match what the N64 runtime expects.
 *
 * Registration order (steps match the in-game call sequence):
 *   1. sithCogFunction_Startup        (generic verbs)
 *   2. sithCogFunctionThing_Startup   (thing verbs)
 *   3. sithCogFunctionAI_Startup      (AI verbs)
 *   4. sithCogFunctionSurface_Startup (surface verbs)
 *   5. sithCogFunctionSound_Startup   (sound verbs)
 *   6. sithCogFunctionSector_Startup  (sector verbs)
 *   7. sithCogFunctionPlayer_Startup  (player verbs)
 *   8. sithCog_Startup messages
 *   9. sithCog_Startup global0..global15
 *  10. jkCog_RegisterVerbs
 *  11. sithCog_StartupEnhanced        (JK13 / !MOTS / !DW verbs)
 *
 * The global symbol table bucket_idx is set to 0x100 before registration
 * begins, so the first verb gets ID 0x100.
 */

#include "jk.h"

/* Thin wrappers copied from sithCog.c lines 1586-1623, re-implemented here
 * to avoid linking the entire sithCog.c dependency tree.  The semantics are
 * identical: NULL function pointer is fine for cogc because we only care
 * about assigning stable IDs to names. */

void sithCogScript_RegisterVerb(sithCogSymboltable *a1,
                                 cogSymbolFunc_t    a2,
                                 const char        *a3)
{
    sithCogStackvar sv;
    sithCogSymbol *sym = sithCogParse_AddSymbol(a1, a3);
    if (sym) {
        sv.type = COG_TYPE_VERB;
        sv.dataAsFunc = a2;
        sithCogParse_SetSymbolVal(sym, &sv);
    }
}

void sithCogScript_RegisterMessageSymbol(sithCogSymboltable *a1,
                                          int32_t             a2,
                                          const char         *a3)
{
    sithCogStackvar sv;
    sithCogSymbol *sym = sithCogParse_AddSymbol(a1, a3);
    if (sym) {
        sv.type = COG_TYPE_INT;
        sv.data[0] = a2;
        sithCogParse_SetSymbolVal(sym, &sv);
    }
}

void sithCogScript_RegisterGlobalMessage(sithCogSymboltable *a1,
                                          const char         *a2,
                                          int32_t             a3)
{
    sithCogStackvar sv;
    sithCogSymbol *sym = sithCogParse_AddSymbol(a1, a2);
    if (sym) {
        sv.type = COG_TYPE_FLEX;
        sv.data[0] = a3;
        sithCogParse_SetSymbolVal(sym, &sv);
    }
}

/* -------------------------------------------------------------------------
 * cogc_RegisterAll
 * Populate the global symbol table with all JK-mode verbs/messages/globals.
 * ctx->bucket_idx must already be set to 0x100 by the caller.
 * ---------------------------------------------------------------------- */
void cogc_RegisterAll(sithCogSymboltable *ctx)
{
#define V(name) sithCogScript_RegisterVerb(ctx, NULL, name)
#define M(id, name) sithCogScript_RegisterMessageSymbol(ctx, id, name)
#define G(name, val) sithCogScript_RegisterGlobalMessage(ctx, name, val)

    /* ------------------------------------------------------------------
     * Step 1: sithCogFunction_Startup (JK-mode, no MOTS/DW block)
     * ----------------------------------------------------------------- */
    V("sleep");
    V("rand");
    V("randvec");
    V("getsenderref");
    V("getsendertype");
    V("getsenderid");
    V("getsourcetype");
    V("getsourceref");
    V("getthingcount");
    V("getgravity");
    V("setgravity");
    V("getleveltime");
    V("getgametime");
    V("getflexgametime");
    V("getthingtemplatecount");
    V("setpulse");
    V("settimer");
    V("settimerex");
    V("killtimerex");
    V("reset");
    V("materialanim");
    V("stopanim");
    V("stopsurfaceanim");
    V("getsurfaceanim");
    V("surfaceanim");
    V("getkeylen");
    V("loadtemplate");
    V("loadkeyframe");
    V("loadmodel");
    V("print");
    V("printint");
    V("printflex");
    V("printvector");
    V("vectoradd");
    V("vectorsub");
    V("vectordot");
    V("vectorcross");
    V("vectorset");
    V("vectorlen");
    V("vectorscale");
    V("vectordist");
    V("vectorx");
    V("vectory");
    V("vectorz");
    V("vectornorm");
    V("getsithmode");
    V("getdifficulty");
    V("setsubmodeflags");
    V("getsubmodeflags");
    V("clearsubmodeflags");
    V("setdebugmodeflags");
    V("getdebugmodeflags");
    V("cleardebugmodeflags");
    V("bitset");
    V("bittest");
    V("bitclear");
    V("fireprojectile");
    V("activateweapon");
    V("deactivateweapon");
    V("setmountwait");
    V("setfirewait");
    V("selectweapon");
    V("assignweapon");
    V("autoselectweapon");
    V("getweaponpriority");
    V("setcurweapon");
    V("getcurweapon");
    V("getcurweaponmode");
    V("changefirerate");
    V("sendmessage");
    V("sendmessageex");
    V("returnex");
    V("getparam");
    V("setparam");
    V("enableirmode");
    V("disableirmode");
    V("setinvflags");
    V("setmapmodeflags");
    V("getmapmodeflags");
    V("clearmapmodeflags");
    V("newcoloreffect");
    V("freecoloreffect");
    /* modifycoloreffect takes the else branch in JK mode: registers
     * sithCogFunction_FreeColorEffect under the name "modifycoloreffect" */
    V("modifycoloreffect");
    V("adddynamictint");
    V("adddynamicadd");
    V("getmaterialcel");
    V("setmaterialcel");
    V("setcamerafocus");
    V("getprimaryfocus");
    V("getsecondaryfocus");
    V("setcurrentcamera");
    V("getcurrentcamera");
    V("cyclecamera");
    V("setpovshake");
    V("setcamerastateflags");
    V("getcamerastateflags");
    V("heapnew");
    V("heapset");
    V("heapget");
    V("heapfree");
    V("getselfcog");
    V("getmastercog");
    V("setmastercog");
    V("setmultimodeflags");
    V("getmultimodeflags");
    V("clearmultimodeflags");
    V("ismulti");
    V("isserver");
    V("setteamscore");
    V("getteamscore");
    V("settimelimit");
    V("gettimelimit");
    V("setscorelimit");
    V("getscorelimit");
    V("sendtrigger");
    V("autosavegame");

    /* ------------------------------------------------------------------
     * Step 2: sithCogFunctionThing_Startup (JK-mode, no MOTS/DW)
     * ----------------------------------------------------------------- */
    V("waitforstop");
    V("stopthing");
    V("destroything");
    V("getthinghealth");
    V("gethealth");
    V("healthing");
    V("getthinglight");
    V("setthinglight");
    V("thinglight");
    V("thinglightanim");
    V("rotate");
    V("creatething");
    /* createthingnr: else branch in JK mode */
    V("createthingnr");
    /* createthingatpos: else branch */
    V("createthingatpos");
    /* createthingatposnr: else branch */
    V("createthingatposnr");
    V("rotatepivot");
    V("capturething");
    V("releasething");
    V("setthingvel");
    V("addthingvel");
    V("applyforce");
    V("detachthing");
    V("getattachflags");
    V("getthingattachflags");
    V("attachthingtosurf");
    V("attachthingtothing");
    V("setarmedmode");
    V("setthingflags");
    V("clearthingflags");
    V("teleportthing");
    V("setthingtype");
    V("setcollidetype");
    V("setheadlightintensity");
    V("getthinggeomode");
    V("setthinggeomode");
    V("getthinglightmode");
    V("setthinglightmode");
    V("getthingtexmode");
    V("setthingtexmode");
    V("getthingcurgeomode");
    V("setthingcurgeomode");
    V("getthingcurlightmode");
    V("setthingcurlightmode");
    V("getthingcurtexmode");
    V("setthingcurtexmode");
    V("setactorextraspeed");
    V("getthingtype");
    V("isthingmoving");
    V("ismoving");
    V("getcurframe");
    V("getgoalframe");
    V("getthingparent");
    V("getthingsector");
    V("getthingpos");
    V("setthingpos");
    V("getthingvel");
    V("getthinguvec");
    V("getthinglvec");
    V("getthingrvec");
    V("getthingflags");
    V("getcollidetype");
    V("getheadlightintensity");
    V("isthingvisible");
    V("setthingpulse");
    V("setthingtimer");
    V("getinv");
    V("setinv");
    V("changeinv");
    V("getinvcog");
    V("getinvmin");
    V("getinvmax");
    /* getcurinvweapon: else branch for JK */
    V("getcurinvweapon");
    V("setcurinvweapon");
    V("playkey");
    V("stopkey");
    V("setthingmodel");
    V("getthingmodel");
    V("playmode");
    V("getmajormode");
    V("firstthinginsector");
    V("nextthinginsector");
    V("prevthinginsector");
    V("movetoframe");
    V("skiptoframe");
    V("jumptoframe");
    V("pathmovepause");
    V("pathmoveresume");
    V("getthingtemplate");
    V("damagething");
    V("setlifeleft");
    V("getlifeleft");
    V("setthingthrust");
    V("getthingthrust");
    V("setthinghealth");
    V("sethealth");
    V("amputatejoint");
    V("setactorweapon");
    /* getactorweapon: else branch for JK */
    V("getactorweapon");
    V("getphysicsflags");
    V("setphysicsflags");
    V("clearphysicsflags");
    V("skilltarget");
    V("parsearg");
    V("getthingrotvel");
    V("setthingrotvel");
    V("setthinglook");
    V("isthingcrouching");
    V("iscrouching");
    V("getthingclasscog");
    V("setthingclasscog");
    V("getthingcapturecog");
    V("setthingcapturecog");
    V("getthingrespawn");
    V("getthingsignature");
    V("setthingattachflags");
    V("clearthingattachflags");
    V("getparticlesize");
    V("setparticlesize");
    V("getparticlegrowthspeed");
    V("setparticlegrowthspeed");
    V("getparticletimeoutrate");
    V("setparticletimeoutrate");
    V("gettypeflags");
    V("settypeflags");
    V("cleartypeflags");
    V("getactorflags");
    V("setactorflags");
    V("clearactorflags");
    V("getweaponflags");
    V("setweaponflags");
    V("clearweaponflags");
    V("getexplosionflags");
    V("setexplosionflags");
    V("clearexplosionflags");
    V("getitemflags");
    V("setitemflags");
    V("clearitemflags");
    V("getparticleflags");
    V("setparticleflags");
    V("clearparticleflags");
    V("takeitem");
    V("haslos");
    V("getthingfireoffset");
    V("setthingfireoffset");
    V("getthinguserdata");
    V("setthinguserdata");
    V("getthingcollidesize");
    V("setthingcollidesize");
    V("getthingmovesize");
    V("setthingmovesize");
    V("getthingmass");
    V("setthingmass");
    V("syncthingpos");
    V("syncthingattachment");
    V("syncthingstate");
    V("attachthingtothingex");

    /* ------------------------------------------------------------------
     * Step 3: sithCogFunctionAI_Startup (JK-mode, no MOTS)
     * ----------------------------------------------------------------- */
    V("aigetmode");
    V("aisetmode");
    V("aiclearmode");
    V("aigetmovepos");
    V("aisetmovepos");
    V("firstthinginview");
    V("nextthinginview");
    V("thingviewdot");
    V("aisetfiretarget");
    V("aisetmovething");
    V("aisetlookpos");
    V("aisetmovespeed");
    V("aisetlookframe");
    V("aisetmoveframe");
    V("isaitargetinsight");
    V("aiflee");
    V("aisetclass");
    V("aijump");

    /* ------------------------------------------------------------------
     * Step 4: sithCogFunctionSurface_Startup (JK-mode, no MOTS vertex light)
     * ----------------------------------------------------------------- */
    V("getsurfaceadjoin");
    V("getsurfacesector");
    V("getnumsurfacevertices");
    V("getsurfacevertexpos");
    V("sethorizonskyoffset");
    V("gethorizonskyoffset");
    V("setceilingskyoffset");
    V("getceilingskyoffset");
    V("slidehorizonsky");
    V("slideceilingsky");
    V("getsurfacecount");
    V("slidewall");
    V("slidesurface");
    V("getwallcel");
    V("setwallcel");
    V("getsurfacecel");
    V("setsurfacecel");
    V("getsurfacemat");
    V("setsurfacemat");
    V("getsurfaceflags");
    V("setsurfaceflags");
    V("clearsurfaceflags");
    V("getadjoinflags");
    V("setadjoinflags");
    V("clearadjoinflags");
    V("setfacetype");
    V("clearfacetype");
    V("getfacetype");
    V("setfacegeomode");
    V("getfacegeomode");
    V("setfacelightmode");
    V("getfacelightmode");
    V("setfacetexmode");
    V("getfacetexmode");
    V("getsurfacelight");
    V("setsurfacelight");
    V("surfacelight");
    V("getsurfacecenter");
    V("surfacecenter");
    V("surfacelightanim");
    V("getsurfacenormal");
    V("syncsurface");

    /* ------------------------------------------------------------------
     * Step 5: sithCogFunctionSound_Startup (JK-mode, no MOTS/DW)
     * ----------------------------------------------------------------- */
    V("playsong");
    V("playsoundthing");
    V("playsoundpos");
    V("playsoundlocal");
    V("playsoundglobal");
    V("stopsound");
    V("loadsound");
    V("playsoundclass");
    V("changesoundvol");
    V("changesoundpitch");
    V("sectorsound");
    V("setmusicvol");
    V("getsoundlen");

    /* ------------------------------------------------------------------
     * Step 6: sithCogFunctionSector_Startup (JK-mode, no DW ambient, no MOTS)
     * ----------------------------------------------------------------- */
    V("getsectortint");
    V("setsectortint");
    V("setsectoradjoins");
    V("sectoradjoins");
    V("getsectorlight");
    V("setsectorlight");
    V("sectorlight");
    V("getcolormap");
    V("getsectorcolormap");
    V("setcolormap");
    V("setsectorcolormap");
    V("getsectorthrust");
    V("setsectorthrust");
    V("sectorthrust");
    V("getsectorflags");
    V("setsectorflags");
    V("clearsectorflags");
    V("getsectorthingcount");
    V("sectorthingcount");
    V("getsectorplayercount");
    V("sectorplayercount");
    V("getsectorcount");
    V("getsectorcenter");
    V("getnumsectorvertices");
    V("getsectorvertexpos");
    V("getnumsectorsurfaces");
    V("getsectorsurfaceref");
    V("syncsector");

    /* ------------------------------------------------------------------
     * Step 7: sithCogFunctionPlayer_Startup (JK-mode, no MOTS killplayerquietly)
     * ----------------------------------------------------------------- */
    V("setinvactivated");
    V("setinvavailable");
    V("isinvactivated");
    V("isinvavailable");
    V("setgoalflags");
    V("cleargoalflags");
    V("getnumplayers");
    V("getmaxplayers");
    V("getabsolutemaxplayers");
    V("getlocalplayerthing");
    V("getplayerthing");
    V("getplayernum");
    V("getplayerteam");
    V("setplayerteam");
    V("getplayerscore");
    V("setplayerscore");
    V("getplayerkills");
    V("setplayerkills");
    V("getplayerkilled");
    V("setplayerkilled");
    V("getplayersuicides");
    V("setplayersuicides");
    V("pickupbackpack");
    V("createbackpack");
    V("nthbackpackbin");
    V("nthbackpackvalue");
    V("numbackpackitems");
    V("getautoswitch");
    V("setautoswitch");
    V("getautopickup");
    V("setautopickup");
    V("getautoreload");
    V("setautoreload");
    V("getrespawnmask");
    V("setrespawnmask");
    V("activatebin");
    V("deactivatebin");
    V("setbinwait");
    V("getnumplayersinteam");
    V("addscoretoteammembers");
    V("syncscores");

    /* ------------------------------------------------------------------
     * Step 8: sithCog_Startup messages (JK-mode, skip DW/MOTS blocks)
     * ----------------------------------------------------------------- */
    M(1,  "activate");
    M(1,  "activated");
    M(3,  "startup");
    M(4,  "timer");
    M(5,  "blocked");
    M(6,  "entered");
    M(7,  "exited");
    M(8,  "crossed");
    M(9,  "sighted");
    M(10, "damaged");
    M(11, "arrived");
    M(12, "killed");
    M(13, "pulse");
    M(14, "touched");
    M(15, "created");
    M(16, "loading");
    M(17, "selected");
    M(18, "deselected");
    M(20, "changed");
    M(21, "deactivated");
    M(22, "shutdown");
    M(23, "respawn");
    M(2,  "removed");
    M(19, "autoselect");
    M(24, "aievent");
    M(25, "skill");
    M(26, "taken");
    M(27, "user0");
    M(28, "user1");
    M(29, "user2");
    M(30, "user3");
    M(31, "user4");
    M(32, "user5");
    M(33, "user6");
    M(34, "user7");
    M(35, "newplayer");
    M(36, "fire");
    M(37, "join");
    M(38, "leave");
    M(39, "splash");
    M(40, "trigger");

    /* ------------------------------------------------------------------
     * Step 9: global0..global15 (sithCog_Startup)
     * ----------------------------------------------------------------- */
    G("global0",  0);
    G("global1",  1);
    G("global2",  2);
    G("global3",  3);
    G("global4",  4);
    G("global5",  5);
    G("global6",  6);
    G("global7",  7);
    G("global8",  8);
    G("global9",  9);
    G("global10", 10);
    G("global11", 11);
    G("global12", 12);
    G("global13", 13);
    G("global14", 14);
    G("global15", 15);

    /* ------------------------------------------------------------------
     * Step 10: jkCog_RegisterVerbs (JK-mode, no MOTS/DW)
     * ----------------------------------------------------------------- */
    V("jkgetlocalplayer");
    V("jksetflags");
    V("jkgetflags");
    V("jkclearflags");
    V("jkendlevel");
    V("jkprintunistring");
    V("jksetpovmodel");
    V("jkplaypovkey");
    V("jkstoppovkey");
    V("jksetweaponmesh");
    V("jkenablesaber");
    V("jkdisablesaber");
    V("jksetsaberinfo");
    V("jksetpersuasioninfo");
    V("jksetforcespeed");
    V("jksetinvis");
    V("jksetinvulnerable");
    V("jksetwaggle");
    V("jksetsuperflags");
    V("jkclearsuperflags");
    V("jkgetsuperflags");
    V("jksettarget");
    V("jkendtarget");
    V("jksettargetcolors");
    V("jkstringclear");
    V("jkstringconcatspace");
    V("jkstringconcatunistring");
    V("jkstringconcatasciistring");
    V("jkstringconcatplayername");
    V("jkstringconcatint");
    V("jkstringconcatformattedint");
    V("jkstringconcatflex");
    V("jkstringconcatformattedflex");
    V("jkstringconcatvector");
    V("jkstringoutput");
    V("jkgetsabercam");
    V("jkgetchoice");

    /* ------------------------------------------------------------------
     * Step 11: sithCog_StartupEnhanced (!MOTS, !DW, JKM_AI)
     *
     * Sub-groups registered in order:
     *   a) !MOTS generic
     *   b) !DW camera
     *   c) !MOTS AI
     *   d) JKM_AI + !MOTS AI extras
     *   e) !MOTS player
     *   f) !MOTS sector
     *   g) !MOTS sound
     *   h) !MOTS surface
     *   i) !MOTS thing
     *   j) !MOTS JK extras
     *   k) !DW DW stubs
     *   l) !MOTS && !DW messages
     *   m) !MOTS && !DW JK13 verbs
     * ----------------------------------------------------------------- */

    /* a) !MOTS generic */
    V("pow");
    V("wakeup");
    V("vectorequal");
    V("fireprojectiledata");
    V("fireprojectilelocal");
    V("getweaponbin");
    V("sendmessageexradius");
    V("worldflash");
    V("setcamerazoom");
    V("getactioncog");
    V("setactioncog");
    V("sin");
    V("cos");
    V("tan");
    V("getcogflags");
    V("setcogflags");
    V("clearcogflags");
    V("debugbreak");
    V("getsysdate");
    V("getsystime");

    /* b) !DW camera */
    V("setcamerafocii");

    /* c) !MOTS AI (firstthingincone, nextthingincone) */
    V("firstthingincone");
    V("nextthingincone");

    /* d) JKM_AI + !MOTS AI extras */
    V("aigetalignment");
    V("aisetalignment");
    V("aisetinterest");
    V("aigetinterest");
    V("aisetdistractor");
    V("aiaddalignmentpriority");
    V("airemovealignmentpriority");

    /* e) !MOTS player */
    V("killplayerquietly");

    /* f) !MOTS sector */
    V("changeallsectorslight");
    V("findsectoratpos");
    V("issphereinsector");
    V("getsectorambientlight");
    V("setsectorambientlight");

    /* g) !MOTS sound */
    V("playsoundthinglocal");
    V("playsoundposlocal");
    V("playvoicething");
    V("playvoicepos");
    V("playvoicelocal");
    V("playvoiceglobal");

    /* h) !MOTS surface vertex light */
    V("getsurfacevertexlight");
    V("setsurfacevertexlight");
    V("getsurfacevertexlightrgb");
    V("setsurfacevertexlightrgb");

    /* i) !MOTS thing */
    V("createthinglocal");
    V("createthingatposowner");
    V("createthingatposold");
    V("setthingparent");
    V("setthingposex");
    V("getthinglvecpyr");
    V("getcurinvweapon2");
    V("getactorweapon2");
    V("setthinglookpyr");
    V("getthingguid");
    V("getguidthing");
    V("getthingmaxvelocity");
    V("setthingmaxvelocity");
    V("getthingmaxangularvelocity");
    V("setthingmaxangularvelocity");
    V("getactorheadpyr");
    V("setactorheadpyr");
    V("setthingjointangle");
    V("getthingjointangle");
    V("setthingmaxheadpitch");
    V("setthingminheadpitch");
    V("interpolatepyr");
    V("setweapontarget");

    /* j) !MOTS JK extras */
    V("jkprintunivoice");
    V("jkgetsabersidemat");
    V("jksyncforcepowers");
    V("jkbegincutscene");
    V("jkendcutscene");
    V("jkstartupcutscene");
    V("jkgetmultiparam");
    V("insideleia");
    V("jkcreatebubble");
    V("jkdestroybubble");
    V("jkgetbubbledistance");
    V("jkthinginbubble");
    V("jkgetfirstbubble");
    V("jkgetnextbubble");
    V("jkgetbubbletype");
    V("jkgetbubbleradius");
    V("jksetbubbletype");
    V("jksetbubbleradius");
    V("jkscreenshot");
    V("jkgetopenframes");

    /* k) !DW stubs */
    V("dwgetactivatebin");
    V("dwsetreftopic");
    V("addbeam");
    V("addlaser");
    V("removelaser");
    V("getlaserid");
    V("dwflashinventory");
    V("dwplaycammyspeech");
    V("dwfreezeplayer");
    V("dwunfreezeplayer");
    V("dwplaycharacterspeech");
    V("dwcleardialog");

    /* l) !MOTS && !DW extra messages */
    M(45, "enterbubble");
    M(46, "exitbubble");
    M(44, "playeraction");
    M(47, "hotkey");

    /* m) !MOTS && !DW JK13 verbs */
    V("getthingattachsurface");
    V("getthingattachthing");
    V("getcamerafov");
    V("getcameraoffset");
    V("setcamerafov");
    V("setcameraoffset");
    V("absolute");
    V("arccosine");
    V("arcsine");
    V("arctangent");
    V("ceiling");
    V("cosine");
    V("floor");
    V("power");
    V("randomflex");
    V("randomint");
    V("sine");
    V("squareroot");
    V("gethotkeycog");
    V("sethotkeycog");
    V("isadjoin");
    V("setgamespeed");
    V("getthingheadlvec");
    V("getthingheadpitch");
    V("getthingheadpyr");
    V("getthingpyr");
    V("setthingheadpyr");
    V("setthingpyr");
    V("setthingrluvecs");
    V("setthingsector");
    V("restorejoint");
    V("getthingairdrag");
    V("getthingeyeoffset");
    V("getthingheadpitchmax");
    V("getthingheadpitchmin");
    V("getthingjumpspeed");
    V("setthingairdrag");
    V("setthingeyeoffset");
    V("setthingheadpitchminmax");
    V("setthingjumpspeed");
    V("setthingmesh");
    V("jksetsaberfaceflags");

#undef V
#undef M
#undef G
}
