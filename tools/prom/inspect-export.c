/* Offline file inspection only. No device, driver, MMIO, or firmware execution.
 * Calls only the immutable parser entrypoint; never calls patch_frts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vbios_610.h"
static unsigned short rd16(const unsigned char *p) {return p[0] | ((unsigned short)p[1]<<8);}
static unsigned int rd32(const unsigned char *p) {return p[0] | ((unsigned int)p[1]<<8) | ((unsigned int)p[2]<<16) | ((unsigned int)p[3]<<24);}
static int within(size_t n, size_t o, size_t k) {return o<=n && k<=n-o;}
int main(int argc, char **argv) {
 FILE *f; long size; unsigned char *b,*saved; size_t n;
 struct gpu_unlock_fwsec fw, invalid;
 unsigned int entries,i,mapper=0,found=0,cmdOff,cmdSize;
 const unsigned char *d,*h,*m; int ret,trunc,bad,shortHeader;
 if(argc!=2) return 2;
 if(fopen_s(&f,argv[1],"rb")) return 3;
 if(fseek(f,0,SEEK_END) || (size=ftell(f))<=0 || size>2097152 || fseek(f,0,SEEK_SET)) {fclose(f);return 4;}
 n=(size_t)size; b=(unsigned char*)malloc(n);saved=(unsigned char*)malloc(n);
 if(!b||!saved) {fclose(f);free(b);free(saved);return 5;}
 if(fread(b,1,n,f)!=n) {fclose(f);free(b);free(saved);return 6;}
 fclose(f);memcpy(saved,b,n);
 ret=gpu_unlock_vbios_fwsec(b,n,&fw);
 if(ret) {printf("{\"parserResult\":%d}\n",ret);free(b);free(saved);return 7;}
 if(memcmp(b,saved,n)) return 8;
 trunc=gpu_unlock_vbios_fwsec(b,n-1,&invalid);
 shortHeader=gpu_unlock_vbios_fwsec(b,25,&invalid);
 b[0]^=1;bad=gpu_unlock_vbios_fwsec(b,n,&invalid);b[0]^=1;
 if(!trunc||!shortHeader||!bad||memcmp(b,saved,n)) return 9;
 if(!within(n,fw.image_off,fw.image_size) || !within(fw.image_size,fw.data_off,fw.dmem_size) || !within(fw.dmem_size,fw.interface_off,4)) return 10;
 d=b+fw.image_off+fw.data_off;h=d+fw.interface_off;entries=h[3];
 if(h[0]!=1||h[1]!=4||h[2]!=8||entries<2||!within(fw.dmem_size,fw.interface_off+4,(size_t)entries*8)) return 11;
 for(i=0;i<entries;i++){
  const unsigned char *e=h+4+i*8;
  if(rd32(e)==4){mapper=rd32(e+4);found++;}
 }
 if(found!=1 || !within(fw.dmem_size,mapper,64)) return 12;
 m=d+mapper;cmdOff=rd32(m+8);cmdSize=rd32(m+12);
 if(rd16(m+4)!=3 || rd16(m+6)<64 || !within(fw.dmem_size,mapper,rd16(m+6)) || cmdSize<44 || !within(fw.dmem_size,cmdOff,cmdSize)) return 13;
#define FIELD(x) printf("\"" #x "\":%u,\n",fw.x)
 printf("{\n\"parserResult\":0,\n");
 FIELD(image_off);FIELD(image_size);FIELD(imem_pa);FIELD(imem_size);FIELD(imem_va);
 FIELD(dmem_pa);FIELD(data_off);FIELD(dmem_size);FIELD(pkc_off);FIELD(interface_off);
 FIELD(ucode_id);FIELD(engine_mask);FIELD(sig_count);FIELD(sig_versions);FIELD(sig_off);FIELD(sig_bytes);
 printf("\"interfaceVersion\":%u,\"interfaceEntries\":%u,\"mapperOffset\":%u,\"mapperVersion\":%u,\"mapperBytes\":%u,\"commandOffset\":%u,\"commandBytes\":%u,\n",h[0],entries,mapper,rd16(m+4),rd16(m+6),cmdOff,cmdSize);
 printf("\"truncatedRejected\":true,\"shortHeaderRejected\":true,\"corruptSignatureRejected\":true,\"inputUnchanged\":true,\"hardwareAccess\":false,\"firmwareSignatureVerified\":false}\n");
 free(b);free(saved);return 0;
}
