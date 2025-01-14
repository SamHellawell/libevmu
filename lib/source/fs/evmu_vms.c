#include <evmu/fs/evmu_vms.h>

#define EVMU_VMS_STRING_GET_(method, field, size) \
    EVMU_EXPORT const char* EvmuVms_##method(const EvmuVms*   pSelf, \
                                            GblStringBuffer* pBuff) \
    { \
        GblStringBuffer_set(pBuff, GBL_STRV(pSelf->field, size)); \
        return GblStringBuffer_cString(pBuff); \
    }

EVMU_VMS_STRING_GET_(vmuDescription, vmuDesc,    EVMU_VMS_VMU_DESCRIPTION_SIZE)
EVMU_VMS_STRING_GET_(dcDescription,  dcDesc,     EVMU_VMS_DC_DESCRIPTION_SIZE)
EVMU_VMS_STRING_GET_(creatorApp,     creatorApp, EVMU_VMS_CREATOR_APP_SIZE)

#define EVMU_VMS_STRING_SET_(method, field, size) \
    EVMU_EXPORT size_t EvmuVms_set##method(EvmuVms* pSelf, const char* pStr) { \
        const size_t copySize = strnlen(pStr, size); \
        memset(pSelf->field, 0, size); \
        memcpy(pSelf->field, pStr, copySize); \
        return copySize; \
    }

EVMU_VMS_STRING_SET_(VmuDescription, vmuDesc,    EVMU_VMS_VMU_DESCRIPTION_SIZE)
EVMU_VMS_STRING_SET_(DcDescription,  dcDesc,     EVMU_VMS_DC_DESCRIPTION_SIZE)
EVMU_VMS_STRING_SET_(CreatorApp,     creatorApp, EVMU_VMS_CREATOR_APP_SIZE)

EVMU_EXPORT GblBool EvmuVms_isValid(const EvmuVms* pSelf) {
    if(pSelf->iconCount    <= EVMU_VMS_ICON_COUNT_MAX &&
       pSelf->eyecatchType <  EVMU_VMS_EYECATCH_COUNT)
    {
        for(unsigned b = 0; b < EVMU_VMS_RESERVED_SIZE; ++b) {
            if(pSelf->reserved[b])
                return GBL_FALSE;
        }

        return GBL_TRUE;
    }

    return GBL_FALSE;
}

EVMU_EXPORT size_t EvmuVms_headerBytes(const EvmuVms* pSelf) {
    //VMS header + icon palette
    size_t size = sizeof(EvmuVms);

    //Icons (Each frame of the animation icon is 512 bytes)
    size += pSelf->iconCount * EVMU_VMS_ICON_BITMAP_SIZE;

    //Eyecatch
    switch(pSelf->eyecatchType) {
    case EVMU_VMS_EYECATCH_16BIT:
        //No palette, all image
        size += EVMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_16BIT;
        break;
    case EVMU_VMS_EYECATCH_PALETTE_256:
        //512 bytes palette, 4032 bytes bitmap
        size += EVMU_VMS_EYECATCH_PALETTE_SIZE_COLOR_256;
        size += EVMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_256;
        break;
    case EVMU_VMS_EYECATCH_PALETTE_16:
        //32 bytes palette, 2016 bytes bitmap
        size += EVMU_VMS_EYECATCH_PALETTE_SIZE_COLOR_16;
        size += EVMU_VMS_EYECATCH_BITMAP_SIZE_COLOR_16;
        break;
    default: //No extra shit if no eyecatch is used
        break;
    }

    return size;
}

EVMU_EXPORT size_t EvmuVms_totalBytes(const EvmuVms* pSelf) {
    return EvmuVms_headerBytes(pSelf) + pSelf->dataBytes;
}


void gyVmuPrintVMSFileInfo(const VMSFileInfo* vms) {
    char string[33];

    assert(sizeof(VMSFileInfo) == VMU_VMS_FILE_INFO_SIZE);

    _gyLog(GY_DEBUG_VERBOSE, "VMS File Info");
    _gyPush();

    memcpy(string, vms->vmuDesc, sizeof(vms->vmuDesc));
    string[sizeof(vms->vmuDesc)] = 0;
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40s", "VMU Description",          string);
    memcpy(string, vms->dcDesc, sizeof(vms->dcDesc));
    string[sizeof(vms->dcDesc)] = 0;
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40s", "DC Description",           string);
    memcpy(string, vms->creatorApp, sizeof(vms->creatorApp));
    string[sizeof(vms->creatorApp)] = 0;
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40s", "Creator Application",      string);
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40u", "Icon Count",               vms->iconCount);
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40u", "Animation Speed",          vms->animSpeed);
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40u", "Eyecatch Type",            vms->eyecatchType);
    //Valid/invalid check!
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40u", "CRC",                      vms->crc);
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40u", "Header Size",              gyVmuVmsFileInfoHeaderSize(vms));
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40u", "Data Size",                vms->dataBytes);
    memcpy(string, vms->reserved, sizeof(vms->reserved));
    string[sizeof(vms->reserved)] = 0;
    _gyLog(GY_DEBUG_VERBOSE, "%-20s: %40s", "Reserved",                 string);

    _gyPop(1);
}


uint16_t** gyVmuVMSFileInfoCreateIconsARGB444(const struct VMUDevice* dev, const struct VMUFlashDirEntry* dirEntry) {
    assert(dev && dirEntry);
    VMSFileInfo* vms = gyVmuFlashDirEntryVmsHeader(dev, dirEntry);
    uint16_t **icons = NULL;

    if(!vms || vms->iconCount == 0) return NULL;
    size_t headerSize = gyVmuVmsFileInfoHeaderSize(vms);
    size_t blockByteRemainder = headerSize % VMU_FLASH_BLOCK_SIZE;
    if(blockByteRemainder) headerSize += (VMU_FLASH_BLOCK_SIZE - blockByteRemainder);

    uint8_t* rawData = malloc(headerSize);
  //  size_t gyVmuFlashFileReadBytes(struct VMUDevice* dev, const struct VMUFlashDirEntry* entry, unsigned char* buffer, uint8_t bytes, uint8_t offset, int includeHeader) {

    const size_t bytesRead = gyVmuFlashFileReadBytes(dev, dirEntry, rawData, headerSize, dirEntry->headerOffset*VMU_FLASH_BLOCK_SIZE, 1);

    if(bytesRead != headerSize) {
        _gyLog(GY_DEBUG_ERROR, "[Creating VMS Icons] Unable to read icon header bytes. Read: [%d/%d]", bytesRead, headerSize);

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

uint16_t* gyVmuVMSFileInfoCreateEyeCatchARGB444(const struct VMUDevice* dev, const struct VMUFlashDirEntry* dirEntry) {
    assert(dev && dirEntry);
    VMSFileInfo* vms = gyVmuFlashDirEntryVmsHeader(dev, dirEntry);

    if(!vms || vms->eyecatchType == VMS_EYECATCH_NONE) return NULL;

    uint16_t* eyecatch = NULL;
    size_t headerSize = gyVmuVmsFileInfoHeaderSize(vms);
    size_t blockByteRemainder = headerSize % VMU_FLASH_BLOCK_SIZE;
    if(blockByteRemainder) headerSize += (VMU_FLASH_BLOCK_SIZE - blockByteRemainder);

    uint8_t* rawData = malloc(headerSize);
    vms = (VMSFileInfo*)rawData;
  //  size_t gyVmuFlashFileReadBytes(struct VMUDevice* dev, const struct VMUFlashDirEntry* entry, unsigned char* buffer, uint8_t bytes, uint8_t offset, int includeHeader) {

    const size_t bytesRead = gyVmuFlashFileReadBytes(dev, dirEntry, rawData, headerSize, dirEntry->headerOffset*VMU_FLASH_BLOCK_SIZE, 1);

    if(bytesRead != headerSize) {
        _gyLog(GY_DEBUG_ERROR, "[Creating VMS Eyecatch] Unable to read header bytes. Read: [%d/%d]", bytesRead, headerSize);
    } else {

       eyecatch = malloc(sizeof(uint16_t)*VMU_VMS_EYECATCH_BITMAP_WIDTH*VMU_VMS_EYECATCH_BITMAP_HEIGHT);

        if(vms->eyecatchType == VMS_EYECATCH_COLOR_16BIT) {
            memcpy(eyecatch, gyVmuVMSFileInfoEyecatch(rawData), VMU_VMS_EYECATCH_BITMAP_WIDTH*VMU_VMS_EYECATCH_BITMAP_HEIGHT*sizeof(uint16_t));
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
    if(_vmsHeaderCheck(image)) return VMU_FLASH_FILE_TYPE_DATA;
    if(_vmsHeaderCheck((char*)image + VMU_FLASH_BLOCK_SIZE)) return VMU_FLASH_FILE_TYPE_GAME;
    return VMU_FLASH_FILE_TYPE_NONE;
}
