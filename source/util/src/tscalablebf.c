/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE

#include "tscalablebf.h"
#include "taoserror.h"
#include "tdef.h"

#define DEFAULT_GROWTH 2
#define DEFAULT_TIGHTENING_RATIO 0.5

static SBloomFilter *tScalableBfAddFilter(SScalableBf *pSBf, uint64_t expectedEntries,
                                   double errorRate);

SScalableBf *tScalableBfInit(uint64_t expectedEntries, double errorRate) {
  const uint32_t defaultSize = 8;
  if (expectedEntries < 1 || errorRate <= 0 || errorRate >= 1.0) {
    return NULL;
  }
  SScalableBf *pSBf = taosMemoryCalloc(1, sizeof(SScalableBf));
  if (pSBf == NULL) {
    return NULL;
  }
  pSBf->numBits = 0;
  pSBf->bfArray = taosArrayInit(defaultSize, POINTER_BYTES);
  if (tScalableBfAddFilter(pSBf, expectedEntries, errorRate * DEFAULT_TIGHTENING_RATIO) == NULL ) {
    tScalableBfDestroy(pSBf);
    return NULL;
  }
  pSBf->growth = DEFAULT_GROWTH;
  return pSBf;
}

int32_t tScalableBfPut(SScalableBf *pSBf, const void *keyBuf, uint32_t len) {
  int32_t size = taosArrayGetSize(pSBf->bfArray);
  for (int32_t i = size - 2; i >= 0; --i) {
    if (tBloomFilterNoContain(taosArrayGetP(pSBf->bfArray, i),
                              keyBuf, len) != TSDB_CODE_SUCCESS) {
      return TSDB_CODE_FAILED;
    }
  }

  SBloomFilter *pNormalBf = taosArrayGetP(pSBf->bfArray, size - 1);
  ASSERT(pNormalBf);
  if (tBloomFilterIsFull(pNormalBf)) {
    pNormalBf = tScalableBfAddFilter(pSBf,
                                     pNormalBf->expectedEntries * pSBf->growth,
                                     pNormalBf->errorRate * DEFAULT_TIGHTENING_RATIO);
    if (pNormalBf == NULL) {
      return TSDB_CODE_OUT_OF_MEMORY;
    }
  }
  return tBloomFilterPut(pNormalBf, keyBuf, len);
}

int32_t tScalableBfNoContain(const SScalableBf *pSBf, const void *keyBuf,
                             uint32_t len) {
  int32_t size = taosArrayGetSize(pSBf->bfArray);
  for (int32_t i = size - 1; i >= 0; --i) {
    if (tBloomFilterNoContain(taosArrayGetP(pSBf->bfArray, i),
                              keyBuf, len) != TSDB_CODE_SUCCESS) {
      return TSDB_CODE_FAILED;
    }
  }
  return TSDB_CODE_SUCCESS;
}

static SBloomFilter *tScalableBfAddFilter(SScalableBf *pSBf, uint64_t expectedEntries,
                                   double errorRate) {
  SBloomFilter *pNormalBf = tBloomFilterInit(expectedEntries, errorRate);
  if (pNormalBf == NULL) {
    return NULL;
  }
  if(taosArrayPush(pSBf->bfArray, &pNormalBf) == NULL) {
    tBloomFilterDestroy(pNormalBf);
    return NULL;
  }
  pSBf->numBits += pNormalBf->numBits;
  return pNormalBf;
}

void tScalableBfDestroy(SScalableBf *pSBf) {
  int32_t size = taosArrayGetSize(pSBf->bfArray);
  for (int32_t i = 0; i < size; ++i) {
    tBloomFilterDestroy(taosArrayGetP(pSBf->bfArray, i));
  }
  taosArrayDestroy(pSBf->bfArray);
  taosMemoryFree(pSBf);
}

void tScalableBfDump(const SScalableBf *pSBf) {
  // Todo;
}