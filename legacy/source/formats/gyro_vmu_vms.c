#include "gyro_vmu_vms.h"
#include "gyro_vmu_flash.h"
#include <evmu/evmu_api.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "fs/evmu_fat_.h"


static int _vmsHeaderCheck(const void* data) {
    VMSFileInfo* info = (VMSFileInfo*)data;

    if(info->iconCount <= VMU_VMS_ICON_COUNT_MAX && info->eyecatchType <= VMS_EYECATCH_MAX) {
        for(unsigned i = 0; i < VMU_VMS_FILE_INFO_RESERVED_SIZE; ++i) {
            if(info->reserved[i]) return 0;
        }
        return 1;
    }
    return 0;
}

uint16_t gyVmuVMSFileInfoCrcCalc(const unsigned char *buf, size_t size, uint16_t* partialCrc) {

    int i, c, n = partialCrc? *partialCrc : 0;
     for (i = 0; i < size; i++) {
       n ^= (buf[i] << 8);
       for (c = 0; c < 8; c++)
         if (n & 0x8000)
       n = (n << 1) ^ 4129;
         else
       n = (n << 1);
     }
     int dun =  n & 0xffff;

    uint16_t crc = partialCrc? *partialCrc : 0;

    for(int i = 0; i < size; ++i) {
        crc ^= (buf[i] << 8);
        for(int j = 0; j < 8; ++j)
            if(crc & 0x8000)
                crc = (crc << 1) ^ 4129;
            else
                crc <<= 1;
    }

    assert(crc == dun);
    return crc;

}

void gyVmuPrintVMSFileInfo(const VMSFileInfo* vms) {
    char string[33];

    assert(sizeof(VMSFileInfo) == VMU_VMS_FILE_INFO_SIZE);

    EVMU_LOG_VERBOSE("VMS File Info");
    EVMU_LOG_PUSH();

    memcpy(string, vms->vmuDesc, sizeof(vms->vmuDesc));
    string[sizeof(vms->vmuDesc)] = 0;
    EVMU_LOG_VERBOSE("%-20s: %40s", "VMU Description",          string);
    memcpy(string, vms->dcDesc, sizeof(vms->dcDesc));
    string[sizeof(vms->dcDesc)] = 0;
    EVMU_LOG_VERBOSE("%-20s: %40s", "DC Description",           string);
    memcpy(string, vms->creatorApp, sizeof(vms->creatorApp));
    string[sizeof(vms->creatorApp)] = 0;
    EVMU_LOG_VERBOSE("%-20s: %40s", "Creator Application",      string);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Icon Count",               vms->iconCount);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Animation Speed",          vms->animSpeed);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Eyecatch Type",            vms->eyecatchType);
    //Valid/invalid check!
    EVMU_LOG_VERBOSE("%-20s: %40u", "CRC",                      vms->crc);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Header Size",              gyVmuVmsFileInfoHeaderSize(vms));
    EVMU_LOG_VERBOSE("%-20s: %40u", "Data Size",                vms->dataBytes);
    memcpy(string, vms->reserved, sizeof(vms->reserved));
    string[sizeof(vms->reserved)] = 0;
    EVMU_LOG_VERBOSE("%-20s: %40s", "Reserved",                 string);

    EVMU_LOG_POP(1);
}

void gyVmuVmsHeaderVmuDescGet(const VMSFileInfo* vms, char* string) {
    memcpy(string, vms->vmuDesc, sizeof(vms->vmuDesc));
    string[sizeof(vms->vmuDesc)] = '\0';
    //These strings aren't NULL terminated, so scoot NULL terminator over to get rid of padding.
    for(int i = sizeof(vms->vmuDesc)-1; i >= 0; --i) {
        if(string[i] != ' ') {
            string[i+1] = '\0';
            break;
        }
    }
}

void gyVmuVmsHeaderDcDescGet(const VMSFileInfo* vms, char* string) {
    memcpy(string, vms->dcDesc, sizeof(vms->dcDesc));
    string[sizeof(vms->dcDesc)] = '\0';
    //These strings aren't NULL terminated, so scoot NULL terminator over to get rid of padding.
    for(int i = sizeof(vms->dcDesc)-1; i >= 0; --i) {
        if(string[i] != ' ') {
            string[i+1] = '\0';
            break;
        }
    }
}

void gyVmuVmsHeaderCreatorAppGet(const VMSFileInfo* vms, char* string) {
    memcpy(string, vms->creatorApp, sizeof(vms->creatorApp));
    string[sizeof(vms->creatorApp)] = '\0';
    //These strings aren't NULL terminated, so scoot NULL terminator over to get rid of padding.
    for(int i = sizeof(vms->creatorApp)-1; i >= 0; --i) {
        if(string[i] != ' ') {
            string[i+1] = '\0';
            break;
        }
    }
}

uint16_t** gyVmuVMSFileInfoCreateIconsARGB444(const EvmuDevice* dev, const EvmuDirEntry* dirEntry) {
    assert(dev && dirEntry);
    VMSFileInfo* vms = (VMSFileInfo*)EvmuFileManager_vms(dev->pFat, dirEntry);
    uint16_t **icons = NULL;

    if(!vms || vms->iconCount == 0) return NULL;
    size_t headerSize = gyVmuVmsFileInfoHeaderSize(vms);
    size_t blockByteRemainder = headerSize % EVMU_FAT_BLOCK_SIZE;
    if(blockByteRemainder) headerSize += (EVMU_FAT_BLOCK_SIZE - blockByteRemainder);

    uint8_t* rawData = malloc(headerSize);
  //  size_t gyVmuFlashFileReadBytes(struct VMUDevice* dev, const struct EvmuDirEntry* entry, unsigned char* buffer, uint8_t bytes, uint8_t offset, int includeHeader) {

    const size_t bytesRead = EvmuFileManager_read(dev->pFat, dirEntry, rawData, headerSize, dirEntry->headerOffset*EVMU_FAT_BLOCK_SIZE, 1);

    if(bytesRead != headerSize) {
        EVMU_LOG_ERROR("[Creating VMS Icons] Unable to read icon header bytes. Read: [%d/%d]", bytesRead, headerSize);

    } else {
        vms = (VMSFileInfo*)rawData;
        icons = malloc(sizeof(uint16_t*)*vms->iconCount);

        for(unsigned i = 0 ; i < vms->iconCount; ++i) {

            icons[i] = malloc(sizeof(uint16_t)*VMU_VMS_ICON_BITMAP_WIDTH*VMU_VMS_ICON_BITMAP_HEIGHT);
            //unsigned char *data = ((unsigned char*)vms)+sizeof(VMSFileInfo);
            uint8_t* data = rawData + sizeof(VMSFileInfo) + i*VMU_VMS_ICON_BITMAP_SIZE;

            for(unsigned b = 0; b < VMU_VMS_ICON_BITMAP_SIZE*2; ++b) {
                const unsigned char palIndex = b%2? data[b/2]&0xf : (data[b/2]>>4)&0xf;
                //assert(palIndex < VMU_VMS_ICON_PALETTE_SIZE);
                icons[i][b] = vms->palette[palIndex];
            }
        }
    }

    free(rawData);

    return icons;
}

void* gyVmuVMSFileInfoEyecatch(const VMSFileInfo *vms) {
    return ((char*)(vms)+sizeof(VMSFileInfo)+vms->iconCount*VMU_VMS_ICON_BITMAP_SIZE);
}

uint16_t* gyVmuVMSFileInfoCreateEyeCatchARGB444(const EvmuDevice* dev, const EvmuDirEntry* dirEntry) {
    assert(dev && dirEntry);
    VMSFileInfo* vms = (VMSFileInfo*)EvmuFileManager_vms(dev->pFat, dirEntry);

    if(!vms || vms->eyecatchType == VMS_EYECATCH_NONE) return NULL;

    uint16_t* eyecatch = NULL;
    size_t headerSize = gyVmuVmsFileInfoHeaderSize(vms);
    size_t blockByteRemainder = headerSize % EVMU_FAT_BLOCK_SIZE;
    if(blockByteRemainder) headerSize += (EVMU_FAT_BLOCK_SIZE - blockByteRemainder);

    uint8_t* rawData = malloc(headerSize);
    vms = (VMSFileInfo*)rawData;
  //  size_t gyVmuFlashFileReadBytes(struct VMUDevice* dev, const struct EvmuDirEntry* entry, unsigned char* buffer, uint8_t bytes, uint8_t offset, int includeHeader) {

    const size_t bytesRead = EvmuFileManager_read(dev->pFat, dirEntry, rawData, headerSize, dirEntry->headerOffset*EVMU_FAT_BLOCK_SIZE, 1);

    if(bytesRead != headerSize) {
        EVMU_LOG_ERROR("[Creating VMS Eyecatch] Unable to read header bytes. Read: [%d/%d]", bytesRead, headerSize);
    } else {

       eyecatch = malloc(sizeof(uint16_t)*VMU_VMS_EYECATCH_BITMAP_WIDTH*VMU_VMS_EYECATCH_BITMAP_HEIGHT);

        if(vms->eyecatchType == VMS_EYECATCH_COLOR_16BIT) {
            memcpy(eyecatch, gyVmuVMSFileInfoEyecatch((const VMSFileInfo*)rawData), VMU_VMS_EYECATCH_BITMAP_WIDTH*VMU_VMS_EYECATCH_BITMAP_HEIGHT*sizeof(uint16_t));
        } else {
            uint16_t*   palette   = gyVmuVMSFileInfoEyecatch(vms);

            if(vms->eyecatchType == VMS_EYECATCH_COLOR_PALETTE_256) {
                uint8_t*    img       = ((uint8_t*)palette)+VMU_VMS_EYECATCH_PALETTE_SIZE_COLOR_PALETTE_256;

                for(unsigned b = 0; b < VMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_PALETTE_256; ++b) {
                    eyecatch[b] = palette[img[b]];
                }
            } else if(vms->eyecatchType == VMS_EYECATCH_COLOR_PALETTE_16) {
                uint8_t*    img       = ((uint8_t*)palette)+VMU_VMS_EYECATCH_PALETTE_SIZE_COLOR_PALETTE_16;


                for(unsigned b = 0; b < VMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_PALETTE_16*2; ++b) {
                    const unsigned char palIndex = b%2? img[b/2]&0xf : (img[b/2]>>4)&0xf;
                    eyecatch[b] = palette[palIndex];
                }
            }
        }
    }

    free(rawData);

    return eyecatch;

}

uint16_t gyVmuVmsFileInfoHeaderSize(const VMSFileInfo* info) {
    //VMS header + icon palette
    uint16_t size = sizeof(VMSFileInfo);

    //Icons
    size += info->iconCount * VMU_VMS_ICON_BITMAP_SIZE; //Each frame of the animation icon is 512 bytes

    //Eyecatch
    switch(info->eyecatchType) {
    case VMS_EYECATCH_COLOR_16BIT:
        size += VMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_16BIT; //No palette, all image
        break;
    case VMS_EYECATCH_COLOR_PALETTE_256:
        size += VMU_VMS_EYECATCH_PALETTE_SIZE_COLOR_PALETTE_256;
        size += VMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_PALETTE_256; //512 bytes palette, 4032 bytes bitmap
        break;
    case VMS_EYECATCH_COLOR_PALETTE_16:
        size += VMU_VMS_EYECATCH_PALETTE_SIZE_COLOR_PALETTE_16;
        size += VMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_PALETTE_16; //32 bytes palette, 2016 bytes bitmap
        break;
    default: //No extra shit if no eyecatch is used
        break;
    }

    return size;
}

/* Try to determine whether VMS file represents GAME or DATA type
 * by trying to determine where the VMS header is located.
 */
int  gyVmuVmsFileInfoType(const void* image) {
    if(_vmsHeaderCheck(image)) return EVMU_FILE_TYPE_DATA;
    if(_vmsHeaderCheck((char*)image + EVMU_FAT_BLOCK_SIZE)) return EVMU_FILE_TYPE_GAME;
    return EVMU_FILE_TYPE_NONE;
}
