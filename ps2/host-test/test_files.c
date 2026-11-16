#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
extern void ps2_prepare_save_dir(void);
extern const char *ps2_save_location(void);
extern FILE *ps2_fopen(const char *,const char *);
static void Put(const char *p){FILE *f=fopen(p,"wb");assert(f);fputs("test",f);assert(!fclose(f));}
int main(int argc,char **argv){
 assert(argc==2);int test=atoi(argv[1]);char tmp[]="/tmp/z3-file-test-XXXXXX";assert(mkdtemp(tmp)&&!chdir(tmp));
 if(test<2){assert(!mkdir("mc0:",0700));assert(!mkdir("mc1:",0700));}
 if(test==1){assert(!mkdir("mc0:/ZELDA3-LTTP",0700));assert(!mkdir("mc1:/ZELDA3-LTTP",0700));Put("mc0:/ZELDA3-LTTP/zelda3.ini");Put("mc1:/ZELDA3-LTTP/sram.dat");}
 ps2_prepare_save_dir();
 if(test==0)assert(!strcmp(ps2_save_location(),"mc0:/ZELDA3-LTTP/"));
 if(test==1){assert(!strcmp(ps2_save_location(),"mc1:/ZELDA3-LTTP/"));assert(!rename("mc1:","disconnected"));assert(!ps2_fopen("saves/sram.a","wb"));assert(!ps2_fopen("saves/zelda3.ini","wb"));assert(access("mc0:/ZELDA3-LTTP/sram.a",F_OK));}
 if(test==2){assert(strstr(ps2_save_location(),"SEM MEMORY CARD"));assert(!mkdir("mc0:",0700));FILE *f=ps2_fopen("saves/sram.a","wb");assert(f);assert(!fclose(f));assert(!access("saves/sram.a",F_OK));assert(access("mc0:/ZELDA3-LTTP/sram.a",F_OK));}
 puts("PASS: save path selection/removal/fallback");return 0;
}
