#include "gyro_vmu_lcd.h"
#include "gyro_vmu_device.h"
#include <assert.h>
#include <stdlib.h>

#include "hw/evmu_device_.h"
#include "hw/evmu_lcd_.h"

static inline size_t _minFileSize(void) {
    return sizeof(LCDFileHeader)+sizeof(LCDCopyright); //assuming you don't have a single frame, which may not even be valid anyway...
}

LCDFile* gyVmuLcdFileLoad(const char* filePath) {
    assert(sizeof(LCDFileHeader) == VMU_LCD_FILE_HEADER_SIZE); //You have a SERIOUS problem here, nigga!
    assert(sizeof(LCDFrameInfo) == VMU_LCD_FRAME_INFO_SIZE);
    assert(sizeof(LCDCopyright) == VMU_LCD_COPYRIGHT_SIZE);

    LCDFile* file = NULL;

    EVMU_LOG_VERBOSE("Loading .LCD animation file [%s]", filePath);
    EVMU_LOG_PUSH();

    FILE* fp = fopen(filePath, "r");

    if(fp) {
        size_t bytes;
        fseek(fp, 0, SEEK_END); // seek to end of file
        bytes = ftell(fp); // get current file pointer
        fseek(fp, 0, SEEK_SET); // seek back to beginning of file

        if(bytes < _minFileSize()) {
            EVMU_LOG_ERROR("Sorry, but there's no fucking way %d bytes is large enough to be a valid .LCD file...", bytes);
            goto closefile;
        }

        file                = malloc(sizeof(LCDFile));
        memset(file, 0, sizeof(LCDFile));
        file->currentFrame  = -1;
        file->fileData      = malloc(bytes);
        file->fileLength    = bytes;
        fread(file->fileData, bytes, 1, fp);

        if(bytes != file->fileLength) {
            EVMU_LOG_ERROR("Unable to read entire file! [%d of %d bytes]", bytes, file->fileLength);
            goto freedata;
        }

        //Validate header data
        file->header        = (LCDFileHeader*)file->fileData;

        char buff[VMU_LCD_SIGNATURE_SIZE+1] = { 0 };
        memcpy(buff, file->header->signature, VMU_LCD_SIGNATURE_SIZE);
        if(strcmp(buff, "LCDi") != 0) {
            EVMU_LOG_ERROR("Unknown file signature: %s", buff);
            goto freedata;
        }

        if(file->header->bitDepth != 1) {
            EVMU_LOG_ERROR("Unsupported bit depth: %d", file->header->bitDepth);
            goto freedata;
        }

        if(file->header->width != EVMU_LCD_PIXEL_WIDTH || file->header->height != EVMU_LCD_PIXEL_HEIGHT) {
            EVMU_LOG_ERROR("Unsupported resolution: <%d, %d>", file->header->width, file->header->height);
            goto freedata;
        }

        const size_t expectedSize = sizeof(LCDFileHeader)
                + file->header->frameCount*sizeof(LCDFrameInfo)
                + file->header->frameCount*VMU_LCD_FRAME_DATA_SIZE
                + sizeof(LCDCopyright);
        if(bytes != expectedSize) {
            EVMU_LOG_ERROR("File size [%d] does not match expected file size [%d]!", bytes, expectedSize);
            goto freedata;
        }

        size_t offset = VMU_LCD_FILE_HEADER_SIZE;

        //Populate frame info pointers
        file->frameInfo = malloc(file->header->frameCount*sizeof(LCDFrameInfo*));
        for(unsigned i = 0; i < file->header->frameCount; ++i) {
            file->frameInfo[i] = (LCDFrameInfo*)&file->fileData[offset];
            offset += VMU_LCD_FRAME_INFO_SIZE;
        }

        //Populate frame data pointers
        file->frameData = malloc(file->header->frameCount*sizeof(uint8_t*));
        for(unsigned i = 0; i < file->header->frameCount; ++i) {
            file->frameData[i] = &file->fileData[offset];
            offset += VMU_LCD_FRAME_DATA_SIZE;
        }

        file->copyright = (LCDCopyright*)&file->fileData[offset];
        offset += VMU_LCD_COPYRIGHT_SIZE;

        assert(offset == bytes); //Something is FUNDAMENTALLY reested otherwise!

        gyVmuLcdPrintFileInfo(file);
        goto closefile;

    } else {
        EVMU_LOG_ERROR("Could not open file!");
        goto done;
    }

freedata:
    free(file->fileData);
    free(file);
    file = NULL;
closefile:
    fclose(fp);
done:
    EVMU_LOG_POP(1);
    return file;
}

void gyVmuLcdFileUnload(LCDFile* lcdFile) {
    if(lcdFile) {
        free(lcdFile->frameInfo);
        free(lcdFile->frameData);
        free(lcdFile->fileData);
        free(lcdFile);
    }
}

void gyVmuLcdPrintFileInfo(const LCDFile* lcdFile) {
    EVMU_LOG_VERBOSE("LCD File Header Info");
    EVMU_LOG_PUSH();

    char buff[VMU_LCD_SIGNATURE_SIZE+1] = { 0 };
    memcpy(buff, lcdFile->header->signature, VMU_LCD_SIGNATURE_SIZE);

    EVMU_LOG_VERBOSE("%-20s: %40s", "Signature",                buff);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Version Number",           lcdFile->header->version);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Frame Width",              lcdFile->header->width);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Frame Height",             lcdFile->header->height);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Bit Depth",                lcdFile->header->bitDepth);
    EVMU_LOG_VERBOSE("%-20s: %40x", "Reserved",                 lcdFile->header->reserved);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Repeat Count",             lcdFile->header->repeatCount);
    EVMU_LOG_VERBOSE("%-20s: %40u", "Frame Count",              lcdFile->header->frameCount);

    char buff2[VMU_LCD_COPYRIGHT_SIZE+1] = { 0 };
    memcpy(buff2, lcdFile->copyright->copyright, VMU_LCD_COPYRIGHT_SIZE);

    EVMU_LOG_VERBOSE("%-20s: %40s", "Copyright",                buff2);


    EVMU_LOG_POP(1);
}

void gyVmuLcdFileFrameStart(struct VMUDevice* dev, uint16_t frameIndex) {
    LCDFile* lcdFile = dev->lcdFile;
    assert(frameIndex < lcdFile->header->frameCount);
    lcdFile->currentFrame   = frameIndex;
    lcdFile->elapsedTime    = 0.0f;
    if(lcdFile->state == LCD_FILE_STATE_COMPLETE) lcdFile->state = LCD_FILE_STATE_STOPPED;

    for(unsigned i = 0; i < VMU_LCD_FRAME_DATA_SIZE; ++i) {
        //EVMU_LOG_VERBOSE("FRAME[%d] = %x", i, lcdFile->frameData[frameIndex]);

        EvmuLcd_setPixel(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pLcd,
                         i%EVMU_LCD_PIXEL_WIDTH, i/EVMU_LCD_PIXEL_WIDTH,
                         (lcdFile->frameData[frameIndex][i] == VMU_LCD_FILE_PIXEL_ON)? 1 : 0);
    }
}

void gyVmuLcdFileUpdate(VMUDevice* dev, float deltaTime) {
    LCDFile* lcdFile = dev->lcdFile;

    if(lcdFile->state == LCD_FILE_STATE_PLAYING) {
        lcdFile->elapsedTime += deltaTime;

        if(lcdFile->elapsedTime >= lcdFile->frameInfo[lcdFile->currentFrame]->delay*VMU_LCD_DELAY_SECS) {
            if(++lcdFile->currentFrame >= lcdFile->header->frameCount) {
                if(lcdFile->header->repeatCount != VMU_LCD_REPEAT_INFINITE && ++lcdFile->loopCount >= lcdFile->header->repeatCount) {
                    --lcdFile->currentFrame;
                    lcdFile->state = LCD_FILE_STATE_COMPLETE;
                    return;
                } else {
                    lcdFile->currentFrame = 0;
                }
            }
            gyVmuLcdFileFrameStart(dev, lcdFile->currentFrame);
        }
    }
}


int gyVmuLcdFileLoadAndStart(VMUDevice *dev, const char *filePath) {
    dev->lcdFile = gyVmuLcdFileLoad(filePath);

    if(dev->lcdFile) {
        EvmuLcd_setScreenEnabled(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pLcd, GBL_TRUE);
        EvmuLcd_setRefreshEnabled(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pLcd, GBL_TRUE);
        gyVmuLcdFileFrameStart(dev, 0);
        EvmuLcd_setIcons(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pLcd, EVMU_LCD_ICONS_NONE);
        dev->lcdFile->state = LCD_FILE_STATE_PLAYING;
        return 1;
    } else {
        return 0;
    }
}

void gyVmuLcdFileStopAndUnload(VMUDevice *dev) {
    gyVmuLcdFileUnload(dev->lcdFile);
    dev->lcdFile = NULL;
}


void gyVmuLcdFileProcessInput(VMUDevice *dev) {
#if 0
    //Start/pause
    if(EvmuGamepad_buttonTapped(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pGamepad,
                                EVMU_GAMEPAD_BUTTON_A)) {
        if(dev->lcdFile->state == LCD_FILE_STATE_PLAYING) {
            dev->lcdFile->state = LCD_FILE_STATE_STOPPED;
        } else if(dev->lcdFile->state == LCD_FILE_STATE_STOPPED) {
            dev->lcdFile->state = LCD_FILE_STATE_PLAYING;
        }
    }

    //Reset
    if(EvmuGamepad_buttonTapped(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pGamepad,
                                EVMU_GAMEPAD_BUTTON_B)) {
        gyVmuLcdFileFrameStart(dev, 0);
        dev->lcdFile->state = LCD_FILE_STATE_PLAYING;
    }

    //Single frame advance/rewind
    if(EvmuGamepad_buttonTapped(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pGamepad,
                                EVMU_GAMEPAD_BUTTON_LEFT)) {
        if(dev->lcdFile->currentFrame > 0)
            gyVmuLcdFileFrameStart(dev, dev->lcdFile->currentFrame-1);
    }
    if(EvmuGamepad_buttonTapped(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pGamepad,
                                EVMU_GAMEPAD_BUTTON_RIGHT)) {
        if(dev->lcdFile->currentFrame+1 < dev->lcdFile->header->frameCount)
            gyVmuLcdFileFrameStart(dev, dev->lcdFile->currentFrame+1);
    }

    //Continuous Advance/Rewind
    if(EvmuGamepad_buttonPressed(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pGamepad,
                                 EVMU_GAMEPAD_BUTTON_UP)) {
        if(dev->lcdFile->currentFrame > 0)
            gyVmuLcdFileFrameStart(dev, dev->lcdFile->currentFrame-1);
    }
    if(EvmuGamepad_buttonPressed(EVMU_DEVICE_PRISTINE_PUBLIC(dev)->pGamepad,
                                EVMU_GAMEPAD_BUTTON_DOWN)) {
        if(dev->lcdFile->currentFrame+1 < dev->lcdFile->header->frameCount)
            gyVmuLcdFileFrameStart(dev, dev->lcdFile->currentFrame+1);
    }
#endif
}
