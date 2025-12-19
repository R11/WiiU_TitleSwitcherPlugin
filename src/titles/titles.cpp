/**
 * Title Management Implementation
 */

#include "titles.h"
#include "../render/image_loader.h"

#include <coreinit/mcp.h>
#include <coreinit/title.h>
#include <nn/acp/title.h>

#include <cstring>
#include <cstdio>
#include <cctype>
#include <malloc.h>

namespace Titles {

namespace {

TitleInfo titleCache[MAX_TITLES];
int titleCount = 0;
bool isLoaded = false;

void getTitleMetadataFromSystem(uint64_t titleId, char* outputName, int maxNameLength,
                                 char* outputProductCode, int maxCodeLength)
{
    if (outputName) {
        snprintf(outputName, maxNameLength, "%016llX", static_cast<unsigned long long>(titleId));
    }
    if (outputProductCode) {
        outputProductCode[0] = '\0';
    }

    ACPMetaXml* metaXml = static_cast<ACPMetaXml*>(memalign(0x40, sizeof(ACPMetaXml)));
    if (!metaXml) {
        return;
    }

    memset(metaXml, 0, sizeof(ACPMetaXml));
    ACPResult result = ACPGetTitleMetaXml(titleId, metaXml);

    if (result == ACP_RESULT_SUCCESS) {
        if (outputName) {
            const char* titleName = nullptr;

            if (metaXml->shortname_en[0] != '\0') {
                titleName = metaXml->shortname_en;
            } else if (metaXml->longname_en[0] != '\0') {
                titleName = metaXml->longname_en;
            } else if (metaXml->shortname_ja[0] != '\0') {
                titleName = metaXml->shortname_ja;
            }

            if (titleName) {
                snprintf(outputName, maxNameLength, "%.63s", titleName);
            }
        }

        if (outputProductCode && metaXml->product_code[0] != '\0') {
            strncpy(outputProductCode, metaXml->product_code, maxCodeLength - 1);
            outputProductCode[maxCodeLength - 1] = '\0';
        }
    }

    free(metaXml);
}

void getTitleNameFromSystem(uint64_t titleId, char* outputName, int maxLength)
{
    getTitleMetadataFromSystem(titleId, outputName, maxLength, nullptr, 0);
}

void sortTitlesAlphabetically()
{
    for (int outerIndex = 0; outerIndex < titleCount - 1; outerIndex++) {
        for (int innerIndex = 0; innerIndex < titleCount - outerIndex - 1; innerIndex++) {
            if (strcasecmp(titleCache[innerIndex].name, titleCache[innerIndex + 1].name) > 0) {
                TitleInfo temporary = titleCache[innerIndex];
                titleCache[innerIndex] = titleCache[innerIndex + 1];
                titleCache[innerIndex + 1] = temporary;
            }
        }
    }
}

}

void Load(bool forceReload)
{
    if (isLoaded && !forceReload) {
        return;
    }

    titleCount = 0;
    isLoaded = false;

    uint64_t currentTitleId = OSGetTitleID();

    int32_t mcpHandle = MCP_Open();
    if (mcpHandle < 0) {
        isLoaded = true;
        return;
    }

    MCPTitleListType* titleListBuffer = static_cast<MCPTitleListType*>(
        malloc(sizeof(MCPTitleListType) * MAX_TITLES)
    );

    if (!titleListBuffer) {
        MCP_Close(mcpHandle);
        isLoaded = true;
        return;
    }

    uint32_t foundTitleCount = 0;
    MCPError mcpError = MCP_TitleListByAppType(
        mcpHandle,
        MCP_APP_TYPE_GAME,
        &foundTitleCount,
        titleListBuffer,
        sizeof(MCPTitleListType) * MAX_TITLES
    );

    if (mcpError >= 0 && foundTitleCount > 0) {
        for (uint32_t listIndex = 0; listIndex < foundTitleCount && titleCount < MAX_TITLES; listIndex++) {
            uint64_t titleId = titleListBuffer[listIndex].titleId;

            if (titleId == currentTitleId) {
                continue;
            }

            titleCache[titleCount].titleId = titleId;
            getTitleMetadataFromSystem(titleId,
                                       titleCache[titleCount].name, MAX_NAME_LENGTH,
                                       titleCache[titleCount].productCode, MAX_PRODUCT_CODE);
            ImageLoader::Request(titleId, ImageLoader::Priority::LOW);
            titleCount++;
        }
    }

    free(titleListBuffer);
    MCP_Close(mcpHandle);
    sortTitlesAlphabetically();
    isLoaded = true;
}

bool IsLoaded()
{
    return isLoaded;
}

void Clear()
{
    titleCount = 0;
    isLoaded = false;
}

int GetCount()
{
    return titleCount;
}

const TitleInfo* GetTitle(int index)
{
    if (index < 0 || index >= titleCount) {
        return nullptr;
    }
    return &titleCache[index];
}

const TitleInfo* FindById(uint64_t titleId)
{
    for (int index = 0; index < titleCount; index++) {
        if (titleCache[index].titleId == titleId) {
            return &titleCache[index];
        }
    }
    return nullptr;
}

int FindIndexById(uint64_t titleId)
{
    for (int index = 0; index < titleCount; index++) {
        if (titleCache[index].titleId == titleId) {
            return index;
        }
    }
    return -1;
}

void GetNameForId(uint64_t titleId, char* outputName, int maxLength)
{
    const TitleInfo* cachedTitle = FindById(titleId);
    if (cachedTitle) {
        strncpy(outputName, cachedTitle->name, maxLength - 1);
        outputName[maxLength - 1] = '\0';
        return;
    }

    getTitleNameFromSystem(titleId, outputName, maxLength);
}

const TitleInfo* FindByProductCode(const char* productCode)
{
    if (!productCode || productCode[0] == '\0') {
        return nullptr;
    }

    int searchLength = strlen(productCode);

    for (int index = 0; index < titleCount; index++) {
        const char* storedCode = titleCache[index].productCode;
        if (storedCode[0] == '\0') continue;

        int storedCodeLength = strlen(storedCode);

        if (strcasecmp(storedCode, productCode) == 0) {
            return &titleCache[index];
        }

        if (searchLength <= storedCodeLength) {
            const char* codeSuffix = storedCode + (storedCodeLength - searchLength);
            if (strcasecmp(codeSuffix, productCode) == 0) {
                return &titleCache[index];
            }
        }

        const char* lastHyphen = strrchr(storedCode, '-');
        if (lastHyphen && strcasecmp(lastHyphen + 1, productCode) == 0) {
            return &titleCache[index];
        }
    }

    return nullptr;
}

}
