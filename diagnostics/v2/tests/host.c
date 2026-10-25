/* SPDX-License-Identifier: MIT. Executes the actual EFI entry against fake protocols. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* EDK2 supplies its own NULL; keep the firmware headers unmodified. */
#undef NULL
#define DIAG_HOST_TEST
#include "../src/diagnostic.c"

static EFI_BOOT_SERVICES Bs;
static EFI_SYSTEM_TABLE Table;
static EFI_PCI_IO_PROTOCOL Gpu[2];
static EFI_LOADED_IMAGE_PROTOCOL Image;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL Fs;
static EFI_FILE_PROTOCOL Root,Marker,File;
static UINT8 Paths[3][16];
static int Usb,MarkerValid,Existing,Full,ShortWrite,FlushError,HeaderError;
static int NoPci,OverLimit,PartialPath,FullConfigError;
static UINT32 ConfigImages[2][64];
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL TextOutputs[2];
static EFI_GRAPHICS_OUTPUT_PROTOCOL Gops[2];
static int Opens,Creates,Writes,Closes,Passed;
static CHAR8 Captured[LOG_CAP];
static CHAR16 WrittenName[64];
#define H(n) ((EFI_HANDLE)(UINTN)(n))
#define CHECK(c) do {if (!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);exit(1);}} while(0)
static int Eq(EFI_GUID *a,EFI_GUID *b) {return memcmp(a,b,sizeof(*a))==0;}
static EFI_STATUS EFIAPI Alloc(EFI_MEMORY_TYPE type,UINTN n,VOID **p) {
    (void)type;*p=malloc(n);return *p?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI Free(VOID *p) {free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Enumerate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **p) {
    (void)type;(void)key;*p=NULL;*n=0;
    if (Eq(g,&PciGuid)) {if(NoPci)return EFI_NOT_FOUND;*n=OverLimit?4097:2;}
    else if (Eq(g,&GopGuid)) *n=2;
    else if (Eq(g,&TextOutGuid)) *n=2;
    else return EFI_NOT_FOUND;
    *p=malloc(*n*sizeof(EFI_HANDLE));CHECK(*p!=NULL);
    for(UINTN i=0;i<*n;++i)(*p)[i]=H(Eq(g,&PciGuid)?i+1:Eq(g,&TextOutGuid)?i+4:i+3);
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Protocol(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
    *p=NULL;
    if(Eq(g,&PciGuid)&&(h==H(1)||h==H(2)))*p=&Gpu[h==H(2)];
    else if(Eq(g,&GopGuid)&&(h==H(3)||h==H(4)))*p=&Gops[h==H(4)];
    else if(Eq(g,&TextOutGuid)&&(h==H(4)||h==H(5)))*p=&TextOutputs[h==H(5)];
    else if(Eq(g,&PathGuid)) {
        if(h==H(1)||h==H(3))*p=Paths[0];
        else if(h==H(2)||h==H(4))*p=Paths[1];
        else if(h==H(7))*p=Paths[2];
    } else if(Eq(g,&ImageGuid)&&h==H(6))*p=&Image;
    else if(Eq(g,&FsGuid)&&h==H(7))*p=&Fs;
    return *p?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Owners(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **p,UINTN *n) {
    *p=NULL;*n=0;
    if(h==H(4)&&Eq(g,&TextOutGuid)) {
        *p=calloc(1,sizeof(**p));CHECK(*p!=NULL);*n=1;
        (*p)->AgentHandle=H(8);(*p)->ControllerHandle=H(5);
        (*p)->Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;(*p)->OpenCount=1;
    }
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Config(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *p) {
    CHECK(w==EfiPciIoWidthUint32);CHECK(off<=0xfc && !(off&3));CHECK(n<=64 && n*4<=256-off);
    if((HeaderError&&off==0)||(FullConfigError&&n==64))return EFI_DEVICE_ERROR;
    memcpy(p,((UINT8 *)ConfigImages[io==&Gpu[1]])+off,n*4);
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
    *seg=0;*bus=io==&Gpu[0]?4:8;*dev=0;*fun=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 bar,UINT64 *support,VOID **resource) {
    (void)io;CHECK(bar<6);*support=0;*resource=NULL;return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f) {(void)f;++Closes;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *buf) {
    CHECK(f==&Marker);CONST CHAR8 *s=MarkerValid?"GPU-DIAG-USB-LOG-v1":"GPU-DIAG-USB-LOG-v1-extra";
    *n=strlen(s);memcpy(buf,s,*n);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *buf) {
    CHECK(f==&File);++Writes;CHECK(*n<LOG_CAP);memcpy(Captured,buf,*n);Captured[*n]=0;
    if(ShortWrite)--*n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Flush(EFI_FILE_PROTOCOL *f) {CHECK(f==&File);return FlushError?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *root,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attr) {
    CHECK(root==&Root);(void)attr;++Opens;*out=NULL;
    if(name[10]=='A') {CHECK(mode==EFI_FILE_MODE_READ);*out=&Marker;return EFI_SUCCESS;}
    CHECK(name[10]=='R'&&name[13]>='0'&&name[16]<='9');
    if(mode==EFI_FILE_MODE_READ) {
        if(Full||Existing>0){if(!Full)--Existing;*out=&File;return EFI_SUCCESS;}
        return EFI_NOT_FOUND;
    }
    CHECK(mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE));
    ++Creates;for(UINTN i=0;i<64;++i){WrittenName[i]=name[i];if(!name[i])break;}
    *out=&File;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *f,EFI_FILE_PROTOCOL **root) {
    CHECK(f==&Fs);*root=&Root;return EFI_SUCCESS;
}
static VOID Setup(VOID) {
    memset(&Bs,0,sizeof(Bs));memset(&Table,0,sizeof(Table));memset(Gpu,0,sizeof(Gpu));
    memset(&Root,0,sizeof(Root));memset(&Marker,0,sizeof(Marker));memset(&File,0,sizeof(File));
    memset(Paths,0,sizeof(Paths));memset(Captured,0,sizeof(Captured));memset(WrittenName,0,sizeof(WrittenName));
    Usb=1;MarkerValid=1;Existing=0;Full=0;ShortWrite=0;FlushError=0;HeaderError=0;NoPci=0;OverLimit=0;PartialPath=0;
    Opens=0;Creates=0;Writes=0;Closes=0;FullConfigError=0;
    memset(ConfigImages,0,sizeof(ConfigImages));memset(TextOutputs,0,sizeof(TextOutputs));memset(Gops,0,sizeof(Gops));
    for(UINTN i=0;i<2;++i) {
        UINT32 *a=ConfigImages[i];UINT8 *c=(UINT8 *)a;
        a[0]=i?0x2c0210de:0x220810de;a[1]=0x00100000;a[2]=0x030000a1;
        a[11]=i?0x176219da:0x1384196e;c[0x34]=0x60;
        c[0x60]=1;c[0x61]=0x68;c[0x68]=5;c[0x69]=0x70;
        c[0x70]=0x11;c[0x71]=0x80;c[0x80]=0x10;c[0x82]=2;c[0x87]=0x10;
    }
    Bs.AllocatePool=Alloc;Bs.FreePool=Free;Bs.HandleProtocol=Protocol;
    Bs.LocateHandleBuffer=Enumerate;Bs.OpenProtocolInformation=Owners;
    Table.BootServices=&Bs;Table.ConsoleOutHandle=H(5);Image.DeviceHandle=H(7);Fs.OpenVolume=Volume;
    Root.Open=Open;Root.Close=Close;Marker.Read=Read;Marker.Close=Close;File.Write=Write;File.Flush=Flush;File.Close=Close;
    for(UINTN i=0;i<2;++i){Gpu[i].Pci.Read=Config;Gpu[i].GetLocation=Location;Gpu[i].GetBarAttributes=Bar;
        Paths[i][0]=1;Paths[i][1]=1;Paths[i][2]=6;Paths[i][5]=(UINT8)(4+4*i);Paths[i][6]=0x7f;Paths[i][7]=0xff;Paths[i][8]=4;}
    Paths[2][0]=3;Paths[2][1]=5;Paths[2][2]=6;Paths[2][6]=0x7f;Paths[2][7]=0xff;Paths[2][8]=4;
}
static VOID Run(CONST char *name) {CHECK(efi_main(H(6),&Table)==EFI_SUCCESS);++Passed;printf("PASS %s\n",name);}
int main(int argc, char **argv) {
    /* Given: dual GPUs. When: real entry runs. Then: unique USB report maps both GOPs. */
    Setup();Run("dual_gpu_inventory");CHECK(Writes==1&&Creates==1);CHECK(strstr(Captured,"id=00000000220810de"));
    CHECK(strstr(Captured,"id=000000002c0210de"));CHECK(strstr(Captured,"GOP_PCI_CANDIDATES=0000000000000001"));
    CHECK(strstr(Captured,"END-OF-INVENTORY"));CHECK(WrittenName[16]=='0');
    if(argc==2) {FILE *sample=NULL;CHECK(fopen_s(&sample,argv[1],"wb")==0);CHECK(fwrite(Captured,1,strlen(Captured),sample)==strlen(Captured));CHECK(fclose(sample)==0);}
    /* Given: internal launch volume. When: entry runs. Then: no filesystem is opened. */
    Setup();Paths[2][1]=1;Run("internal_volume_never_opened");CHECK(Opens==0&&Writes==0);
    /* Given: a malformed USB node. When: entry runs. Then: no disk is opened. */
    Setup();Paths[2][2]=4;Run("malformed_usb_never_opened");CHECK(Opens==0&&Writes==0);
    /* Given: marker with extra bytes. When: entry runs. Then: no file is created. */
    Setup();MarkerValid=0;Run("invalid_marker_rejected");CHECK(Creates==0&&Writes==0);
    /* Given: an existing report. When: entry runs. Then: next filename is selected. */
    Setup();Existing=1;Run("existing_log_not_overwritten");CHECK(Creates==1&&WrittenName[16]=='1');
    /* Given: all names occupied. When: save runs. Then: no write and explicit failure. */
    Setup();Full=1;St=&Table;Log=malloc(LOG_CAP);Used=0;Log[0]=0;
    CHECK(SaveLog(H(6))==EFI_VOLUME_FULL&&Creates==0&&Writes==0);free(Log);Log=NULL;++Passed;puts("PASS log_namespace_full");
    /* Given: short disk write. When: save runs. Then: it cannot report success. */
    Setup();ShortWrite=1;St=&Table;Log=malloc(LOG_CAP);Used=4;memcpy(Log,"test",5);
    CHECK(SaveLog(H(6))==EFI_DEVICE_ERROR);free(Log);Log=NULL;++Passed;puts("PASS short_write_detected");
    /* Given: flush failure. When: save runs. Then: it cannot report success. */
    Setup();FlushError=1;St=&Table;Log=malloc(LOG_CAP);Used=4;memcpy(Log,"test",5);
    CHECK(SaveLog(H(6))==EFI_DEVICE_ERROR);free(Log);Log=NULL;++Passed;puts("PASS flush_failure_detected");
    /* Given: config read error. When: entry runs. Then: report preserves the errors. */
    Setup();HeaderError=1;Run("pci_read_failure_reported");CHECK(strstr(Captured,"ERROR pci-header"));
    /* Given: no PCI protocol handles. When: entry runs. Then: absence remains explicit. */
    Setup();NoPci=1;Run("no_pci_reported");CHECK(strstr(Captured,"PCI_HANDLE_COUNT=0000000000000000"));
    /* Given: excessive firmware handle count. When: entry runs. Then: processing is bounded. */
    Setup();OverLimit=1;Run("handle_limit_rejected");CHECK(strstr(Captured,"ERROR handle-count"));
    /* Given: malformed and multi-instance paths. When: parser runs. Then: neither is accepted. */
    Setup();UINTN len=0;BOOLEAN usb=FALSE;Paths[0][2]=3;CHECK(!PathLength(Paths[0],&len,&usb));
    Paths[0][2]=6;Paths[0][7]=1;CHECK(!PathLength(Paths[0],&len,&usb));++Passed;puts("PASS invalid_paths_rejected");
    /* Given: nearly full output. When: text is appended. Then: overflow is detected and bounded. */
    Setup();Log=malloc(LOG_CAP);Used=LOG_CAP-2;Truncated=FALSE;Text("abcd");CHECK(Truncated&&Used==LOG_CAP-1&&Log[Used]==0);
    free(Log);Log=NULL;++Passed;puts("PASS output_overflow_detected");
    /* New evidence and rejection scenarios execute the same firmware entry. */
    Setup();Run("full_config_and_capability_decode");
    CHECK(strstr(Captured,"CONFIG256 handle=")&&strstr(Captured,"CFG_DWORD offset="));
    CHECK(strstr(Captured,"CAP_CHAIN_COMPLETE count=")&&strstr(Captured,"flr-capable="));
    CHECK(strstr(Captured,"PM_CSR=")&&strstr(Captured,"MSI_ENABLED=")&&strstr(Captured,"MSIX_ENABLED="));
    {CHAR8 expected[100];sprintf_s(expected,sizeof(expected),"flr-capable=%016llx",1ULL);CHECK(strstr(Captured,expected));}
    {UINTN words=0;CONST CHAR8 *scan=Captured;while((scan=strstr(scan," CFG_DWORD offset="))!=NULL){++words;++scan;}CHECK(words==128);}
    Setup();FullConfigError=1;Run("full_config_read_failure");CHECK(strstr(Captured,"ERROR config256-read"));
    Setup();((UINT8 *)ConfigImages[0])[0x81]=0x60;Run("capability_cycle_rejected");CHECK(strstr(Captured,"ERROR capability-pointer-or-cycle"));
    Setup();((UINT8 *)ConfigImages[0])[0x34]=0x41;Run("unaligned_capability_rejected");CHECK(strstr(Captured,"ERROR capability-pointer-or-cycle"));
    Setup();((UINT8 *)ConfigImages[0])[0x34]=0x20;Run("header_capability_pointer_rejected");CHECK(strstr(Captured,"ERROR capability-pointer-or-cycle"));
    Setup();((UINT8 *)ConfigImages[0])[0x34]=0xf0;((UINT8 *)ConfigImages[0])[0xf0]=0x10;
    Run("pcie_capability_overrun_rejected");CHECK(strstr(Captured,"ERROR pcie-capability-bounds"));
    Setup();((UINT8 *)ConfigImages[0])[0x34]=0xfc;((UINT8 *)ConfigImages[0])[0xfc]=1;
    Run("pm_capability_overrun_rejected");CHECK(strstr(Captured,"ERROR pm-capability-bounds"));
    Setup();((UINT8 *)ConfigImages[0])[0x0e]=1;Run("non_type_zero_display_rejected");CHECK(strstr(Captured,"ERROR display-header-type"));
    Setup();ConfigImages[0][1]=0;Run("absent_capability_bit_is_explicit");CHECK(strstr(Captured,"CAP_CHAIN=ABSENT"));
    Setup();((UINT8 *)ConfigImages[0])[0x60]=0xfe;Run("unknown_capability_preserved");CHECK(!strstr(Captured,"ERROR "));
    Setup();Run("physical_to_aggregate_console_edge");
    CHECK(strstr(Captured,"TEXT_OUT_COUNT=")&&strstr(Captured,"TEXT_PCI_CANDIDATES="));
    CHECK(strstr(Captured,"AGENT=")&&strstr(Captured,"controller=")&&strstr(Captured,"CONSOLE_GRAPH_POLICY="));
    {CHAR8 expected[100];sprintf_s(expected,sizeof(expected),"TEXT_PCI_CANDIDATES=%016llx owner=%016llx",1ULL,2ULL);CHECK(strstr(Captured,expected));}
    {CHAR8 expected[100];sprintf_s(expected,sizeof(expected),"controller=%016llx attributes=%016llx",5ULL,(unsigned long long)EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);CHECK(strstr(Captured,expected));}
    Setup();Paths[0][5]=Paths[1][5];Run("ambiguous_console_mapping_not_selected");
    {CHAR8 expected[100];sprintf_s(expected,sizeof(expected),"TEXT_PCI_CANDIDATES=%016llx owner=%016llx",2ULL,0ULL);CHECK(strstr(Captured,expected));}
    printf("%d scenarios passed; host fixtures are not hardware validation.\n",Passed);return 0;
}
