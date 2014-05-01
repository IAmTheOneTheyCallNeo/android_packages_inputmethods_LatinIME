/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "suggest/policyimpl/dictionary/structure/v4/content/bigram_dict_content.h"

#include "suggest/policyimpl/dictionary/utils/buffer_with_extendable_buffer.h"

namespace latinime {

const BigramEntry BigramDictContent::getBigramEntryAndAdvancePosition(
        int *const bigramEntryPos) const {
    const BufferWithExtendableBuffer *const bigramListBuffer = getContentBuffer();
    if (*bigramEntryPos < 0 || *bigramEntryPos >=  bigramListBuffer->getTailPosition()) {
        AKLOGE("Invalid bigram entry position. bigramEntryPos: %d, bufSize: %d",
                *bigramEntryPos, bigramListBuffer->getTailPosition());
        ASSERT(false);
        return BigramEntry(false /* hasNext */, NOT_A_PROBABILITY,
                Ver4DictConstants::NOT_A_TERMINAL_ID);
    }
    const int bigramFlags = bigramListBuffer->readUintAndAdvancePosition(
            Ver4DictConstants::BIGRAM_FLAGS_FIELD_SIZE, bigramEntryPos);
    const bool hasNext = (bigramFlags & Ver4DictConstants::BIGRAM_HAS_NEXT_MASK) != 0;
    int probability = NOT_A_PROBABILITY;
    int timestamp = NOT_A_TIMESTAMP;
    int level = 0;
    int count = 0;
    if (mHasHistoricalInfo) {
        probability = bigramListBuffer->readUintAndAdvancePosition(
                Ver4DictConstants::PROBABILITY_SIZE, bigramEntryPos);
        timestamp = bigramListBuffer->readUintAndAdvancePosition(
                Ver4DictConstants::TIME_STAMP_FIELD_SIZE, bigramEntryPos);
        level = bigramListBuffer->readUintAndAdvancePosition(
                Ver4DictConstants::WORD_LEVEL_FIELD_SIZE, bigramEntryPos);
        count = bigramListBuffer->readUintAndAdvancePosition(
                Ver4DictConstants::WORD_COUNT_FIELD_SIZE, bigramEntryPos);
    } else {
        probability = bigramFlags & Ver4DictConstants::BIGRAM_PROBABILITY_MASK;
    }
    const int encodedTargetTerminalId = bigramListBuffer->readUintAndAdvancePosition(
            Ver4DictConstants::BIGRAM_TARGET_TERMINAL_ID_FIELD_SIZE, bigramEntryPos);
    const int targetTerminalId =
            (encodedTargetTerminalId == Ver4DictConstants::INVALID_BIGRAM_TARGET_TERMINAL_ID) ?
                    Ver4DictConstants::NOT_A_TERMINAL_ID : encodedTargetTerminalId;
    if (mHasHistoricalInfo) {
        const HistoricalInfo historicalInfo(timestamp, level, count);
        return BigramEntry(hasNext, probability, &historicalInfo, targetTerminalId);
    } else {
        return BigramEntry(hasNext, probability, targetTerminalId);
    }
}

bool BigramDictContent::writeBigramEntryAndAdvancePosition(
        const BigramEntry *const bigramEntryToWrite, int *const entryWritingPos) {
    BufferWithExtendableBuffer *const bigramListBuffer = getWritableContentBuffer();
    const int bigramFlags = createAndGetBigramFlags(
            mHasHistoricalInfo ? 0 : bigramEntryToWrite->getProbability(),
            bigramEntryToWrite->hasNext());
    if (!bigramListBuffer->writeUintAndAdvancePosition(bigramFlags,
            Ver4DictConstants::BIGRAM_FLAGS_FIELD_SIZE, entryWritingPos)) {
        AKLOGE("Cannot write bigram flags. pos: %d, flags: %x", *entryWritingPos, bigramFlags);
        return false;
    }
    if (mHasHistoricalInfo) {
        if (!bigramListBuffer->writeUintAndAdvancePosition(bigramEntryToWrite->getProbability(),
                Ver4DictConstants::PROBABILITY_SIZE, entryWritingPos)) {
            AKLOGE("Cannot write bigram probability. pos: %d, probability: %d", *entryWritingPos,
                    bigramEntryToWrite->getProbability());
            return false;
        }
        const HistoricalInfo *const historicalInfo = bigramEntryToWrite->getHistoricalInfo();
        if (!bigramListBuffer->writeUintAndAdvancePosition(historicalInfo->getTimeStamp(),
                Ver4DictConstants::TIME_STAMP_FIELD_SIZE, entryWritingPos)) {
            AKLOGE("Cannot write bigram timestamps. pos: %d, timestamp: %d", *entryWritingPos,
                    historicalInfo->getTimeStamp());
            return false;
        }
        if (!bigramListBuffer->writeUintAndAdvancePosition(historicalInfo->getLevel(),
                Ver4DictConstants::WORD_LEVEL_FIELD_SIZE, entryWritingPos)) {
            AKLOGE("Cannot write bigram level. pos: %d, level: %d", *entryWritingPos,
                    historicalInfo->getLevel());
            return false;
        }
        if (!bigramListBuffer->writeUintAndAdvancePosition(historicalInfo->getCount(),
                Ver4DictConstants::WORD_COUNT_FIELD_SIZE, entryWritingPos)) {
            AKLOGE("Cannot write bigram count. pos: %d, count: %d", *entryWritingPos,
                    historicalInfo->getCount());
            return false;
        }
    }
    const int targetTerminalIdToWrite =
            (bigramEntryToWrite->getTargetTerminalId() == Ver4DictConstants::NOT_A_TERMINAL_ID) ?
                    Ver4DictConstants::INVALID_BIGRAM_TARGET_TERMINAL_ID :
                            bigramEntryToWrite->getTargetTerminalId();
    if (!bigramListBuffer->writeUintAndAdvancePosition(targetTerminalIdToWrite,
            Ver4DictConstants::BIGRAM_TARGET_TERMINAL_ID_FIELD_SIZE, entryWritingPos)) {
        AKLOGE("Cannot write bigram target terminal id. pos: %d, target terminal id: %d",
                *entryWritingPos, bigramEntryToWrite->getTargetTerminalId());
        return false;
    }
    return true;
}

bool BigramDictContent::copyBigramList(const int bigramListPos, const int toPos,
        int *const outTailEntryPos) {
    int readingPos = bigramListPos;
    int writingPos = toPos;
    bool hasNext = true;
    while (hasNext) {
        const BigramEntry bigramEntry = getBigramEntryAndAdvancePosition(&readingPos);
        hasNext = bigramEntry.hasNext();
        if (!hasNext) {
            *outTailEntryPos = writingPos;
        }
        if (!writeBigramEntryAndAdvancePosition(&bigramEntry, &writingPos)) {
            AKLOGE("Cannot write bigram entry to copy. pos: %d", writingPos);
            return false;
        }
    }
    return true;
}

bool BigramDictContent::runGC(const TerminalPositionLookupTable::TerminalIdMap *const terminalIdMap,
        const BigramDictContent *const originalBigramDictContent,
        int *const outBigramEntryCount) {
    for (TerminalPositionLookupTable::TerminalIdMap::const_iterator it = terminalIdMap->begin();
            it != terminalIdMap->end(); ++it) {
        const int originalBigramListPos =
                originalBigramDictContent->getBigramListHeadPos(it->first);
        if (originalBigramListPos == NOT_A_DICT_POS) {
            // This terminal does not have a bigram list.
            continue;
        }
        const int bigramListPos = getContentBuffer()->getTailPosition();
        int bigramEntryCount = 0;
        // Copy bigram list with GC from original content.
        if (!runGCBigramList(originalBigramListPos, originalBigramDictContent, bigramListPos,
                terminalIdMap, &bigramEntryCount)) {
            AKLOGE("Cannot complete GC for the bigram list. original pos: %d, pos: %d",
                    originalBigramListPos, bigramListPos);
            return false;
        }
        if (bigramEntryCount == 0) {
            // All bigram entries are useless. This terminal does not have a bigram list.
            continue;
        }
        *outBigramEntryCount += bigramEntryCount;
        // Set bigram list position to the lookup table.
        if (!getUpdatableAddressLookupTable()->set(it->second, bigramListPos)) {
            AKLOGE("Cannot set bigram list position. terminal id: %d, pos: %d",
                    it->second, bigramListPos);
            return false;
        }
    }
    return true;
}

// Returns whether GC for the bigram list was succeeded or not.
bool BigramDictContent::runGCBigramList(const int bigramListPos,
        const BigramDictContent *const sourceBigramDictContent, const int toPos,
        const TerminalPositionLookupTable::TerminalIdMap *const terminalIdMap,
        int *const outEntrycount) {
    bool hasNext = true;
    int readingPos = bigramListPos;
    int writingPos = toPos;
    int lastEntryPos = NOT_A_DICT_POS;
    while (hasNext) {
        const BigramEntry originalBigramEntry =
                sourceBigramDictContent->getBigramEntryAndAdvancePosition(&readingPos);
        hasNext = originalBigramEntry.hasNext();
        if (originalBigramEntry.getTargetTerminalId() == Ver4DictConstants::NOT_A_TERMINAL_ID) {
            continue;
        }
        TerminalPositionLookupTable::TerminalIdMap::const_iterator it =
                terminalIdMap->find(originalBigramEntry.getTargetTerminalId());
        if (it == terminalIdMap->end()) {
            // Target word has been removed.
            continue;
        }
        lastEntryPos = hasNext ? writingPos : NOT_A_DICT_POS;
        const BigramEntry updatedBigramEntry =
                originalBigramEntry.updateTargetTerminalIdAndGetEntry(it->second);
        if (!writeBigramEntryAndAdvancePosition(&updatedBigramEntry, &writingPos)) {
            AKLOGE("Cannot write bigram entry to run GC. pos: %d", writingPos);
            return false;
        }
        *outEntrycount += 1;
    }
    if (lastEntryPos != NOT_A_DICT_POS) {
        // Update has next flag in the last written entry.
        const BigramEntry bigramEntry = getBigramEntry(lastEntryPos).updateHasNextAndGetEntry(
                false /* hasNext */);
        if (!writeBigramEntry(&bigramEntry, lastEntryPos)) {
            AKLOGE("Cannot write bigram entry to set hasNext flag after GC. pos: %d", writingPos);
            return false;
        }
    }
    return true;
}

} // namespace latinime
