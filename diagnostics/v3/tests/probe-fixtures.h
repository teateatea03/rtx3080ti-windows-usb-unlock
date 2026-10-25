/* SPDX-License-Identifier: MIT. Real entrypoint / transaction failure injection. */
#include "observed.h"
static EFI_STATUS EFIAPI CommandWrite(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 off,UINTN n,VOID *data) {
    UINT16 value=0;CHECK(ProbeFixture&&io==&Gpu[0]);CHECK(width==EfiPciIoWidthUint16&&off==4&&n==1);
    memcpy(&value,data,2);CHECK(value==0||value==2);CHECK(PreFlushed);++ConfigWrites;
    if(value==0&&RestoreErrors){if(RestoreErrors>0)--RestoreErrors;return EFI_DEVICE_ERROR;}
    if(!(value==2&&EnableNoEffect))memcpy(((UINT8 *)ConfigImages[0])+4,&value,2);
    if(value==2&&EnableError)return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI MemoryRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 off,UINTN n,VOID *data) {
    CHECK(ProbeFixture&&io==&Gpu[0]&&width==EfiPciIoWidthUint32&&bar==0);
    CHECK((ConfigImages[0][1]&0xffff)==2&&PreFlushed);++MemoryReads;
    if(MemErrorAt&&MemoryReads==MemErrorAt)return EFI_DEVICE_ERROR;
    if(n==1024){CHECK(off>=0x300000&&off<=0x3ff000&&!(off&0xfff));++PromPages;
        if(PromErrorAt&&PromPages==PromErrorAt)return EFI_DEVICE_ERROR;
        memset(data,(int)((off>>12)&255),n*4);return EFI_SUCCESS;}
    CHECK(n==1);BOOLEAN allowed=FALSE;
    for(UINTN i=0;i<sizeof(ProbeOffsets)/sizeof(ProbeOffsets[0]);++i)if(off==ProbeOffsets[i])allowed=TRUE;
    CHECK(allowed);UINT32 value=off==0?(BadChip?0xffffffffU:0xb72000a1U):off==0x1183a4?0x3000U:0;
    memcpy(data,&value,4);return EFI_SUCCESS;
}
static VOID SetupProbe(VOID) {
    Setup();ProbeFixture=1;Table.StandardErrorHandle=H(5);
    memcpy(ConfigImages[0],ObservedTarget,sizeof(ObservedTarget));ConfigImages[1][1]|=7;
    Gpu[0].Pci.Write=CommandWrite;Gpu[0].Mem.Read=MemoryRead;
}
static VOID Refused(CONST char *name) {Run(name);CHECK(ConfigWrites==0&&MemoryReads==0&&PromFiles==0);}
static VOID Restored(CONST char *name) {Run(name);CHECK(ConfigWrites>=2&&(ConfigImages[0][1]&0xffff)==0&&!ProbeRestoreFailed);}
static VOID ProbeScenarios(CONST char *samplePath) {
    SetupProbe();Restored("probe_success_and_exact_restore");
    CHECK(ConfigWrites==2&&MemoryReads==268&&PromPages==256&&PromFiles==1&&Writes==3);
    CHECK((ConfigImages[0][1]&0xffff0000U)==0x00100000U);
    CHECK(strstr(Captured,"PROBE_CONFIG256_UNCHANGED=")&&strstr(Captured,"PROBE_PROM_SAVE_STATUS="));
    if(samplePath){
        FILE *f=NULL;CHECK(fopen_s(&f,samplePath,"wb")==0);CHECK(fwrite(Captured,1,strlen(Captured),f)==strlen(Captured));CHECK(fclose(f)==0);
        char path[1024];CHECK(strcpy_s(path,sizeof(path),samplePath)==0);char *slash=strrchr(path,'\\');CHECK(slash!=NULL);
        CHECK(strcpy_s(slash+1,sizeof(path)-(size_t)(slash+1-path),"PRM0000.BIN")==0);
        CHECK(fopen_s(&f,path,"wb")==0);CHECK(fwrite(CapturedProm,1,PROBE_PROM_BYTES,f)==PROBE_PROM_BYTES);CHECK(fclose(f)==0);
    }
    SetupProbe();ConfigImages[0][11]^=1;Refused("wrong_subsystem_never_written");
    SetupProbe();ConfigImages[0][1]|=4;Refused("bus_master_on_refused");
    SetupProbe();ConfigImages[0][1]|=2;Refused("unexpected_memory_decode_refused");
    SetupProbe();ConfigImages[0][4]=0xe0000000;Refused("wrong_bar_config_refused");
    SetupProbe();ProbeBadBar=1;Refused("wrong_bar_resource_refused");
    SetupProbe();ProbeOwned=1;Refused("target_driver_or_child_refused");
    SetupProbe();ProbeWrongBus=1;Refused("wrong_target_location_refused");
    SetupProbe();ProbeMissingEdge=1;Refused("missing_console_edge_refused");
    SetupProbe();ProbeExtraGop=1;Refused("target_or_extra_gop_refused");
    SetupProbe();MarkerValid=0;Refused("invalid_usb_marker_prevents_config_writes");
    SetupProbe();Paths[2][1]=1;Refused("internal_volume_prevents_config_writes");
    SetupProbe();FlushError=1;Refused("preflight_flush_failure_prevents_config_writes");
    SetupProbe();PoolError=1;Refused("prom_allocation_failure_prevents_config_writes");
    SetupProbe();NullReadOpen=1;Refused("invalid_success_null_file_prevents_config_writes");
    SetupProbe();PreCloseError=1;Refused("preflight_close_failure_prevents_config_writes");
    SetupProbe();MutatePre=1;Refused("state_changed_during_usb_callback_refused");
    SetupProbe();EnableError=1;Restored("enable_write_error_still_restores");CHECK(MemoryReads==0);
    SetupProbe();EnableNoEffect=1;Restored("enable_not_latched_no_mmio");CHECK(MemoryReads==0);
    SetupProbe();CommandReadError=1;Restored("enable_readback_error_restores");CHECK(MemoryReads==0);
    SetupProbe();BadChip=1;Restored("inaccessible_chip_stops_and_restores");CHECK(MemoryReads==1&&PromFiles==0);
    SetupProbe();MemErrorAt=4;Restored("register_read_error_restores");CHECK(PromFiles==0);
    SetupProbe();PromErrorAt=5;Restored("partial_prom_never_saved");CHECK(PromPages==5&&PromFiles==0);
    SetupProbe();RestoreErrors=1;Restored("restore_retry_is_bounded");CHECK(ConfigWrites==3);
    SetupProbe();RestoreErrors=-1;Run("unverified_restore_sets_fail_stop");CHECK(ProbeRestoreFailed&&ConfigWrites==3&&PromFiles==0);
    SetupProbe();RestoreReadError=1;Run("restore_readback_failure_also_stops_boot");CHECK(ProbeRestoreFailed&&ConfigWrites==3&&PromFiles==0&&(ConfigImages[0][1]&0xffff)==0);
    SetupProbe();memcpy(ConfigImages[1],ObservedTarget,sizeof(ObservedTarget));Refused("duplicate_target_identity_refused");
    SetupProbe();UINT32 word=0;CHECK(ProbeRead(0,1,&word)==EFI_ACCESS_DENIED);
    CHECK(ProbeWriteCommand(2)==EFI_ACCESS_DENIED);ProbeActive=TRUE;ProbeIo=&Gpu[0];
    CHECK(ProbeRead(0x823818,1,&word)==EFI_ACCESS_DENIED);
    CHECK(ProbeRead(0x400000,1024,&word)==EFI_ACCESS_DENIED);
    CHECK(ProbeRead(0x300004,1024,&word)==EFI_ACCESS_DENIED);
    CHECK(ProbeRead(0x300000,1025,&word)==EFI_ACCESS_DENIED);
    CHECK(ProbeWriteCommand(4)==EFI_ACCESS_DENIED);
    CHECK(ConfigWrites==0&&MemoryReads==0);ProbeActive=FALSE;ProbeIo=NULL;++Passed;puts("PASS inactive_and_non_whitelist_access_denied");
}
