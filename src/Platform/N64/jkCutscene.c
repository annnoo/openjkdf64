// N64 stub: cutscenes (smacker/smush) not supported on N64
#include "Main/jkCutscene.h"
#include "jk.h"

void jkCutscene_Startup(char *fpath) {}
void jkCutscene_Shutdown() {}
int  jkCutscene_sub_421310(char* a1) { return 0; }
int  jkCutscene_sub_421410() { return 0; }
int  jkCutscene_smack_related_loops() { return 0; }
int  jkCutscene_PauseShow(int unk) { return 0; }
int  jkCutscene_Handler(HWND a1, UINT a2, WPARAM a3, LPARAM a4, LRESULT *a5) { return 0; }
int  jkCutscene_smacker_process() { return 0; }
int  jkCutscene_smusher_process() { return 0; }
