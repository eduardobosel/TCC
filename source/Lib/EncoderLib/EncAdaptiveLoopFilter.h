/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2023, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     EncAdaptiveLoopFilter.h
 \brief    estimation part of adaptive loop filter class (header)
 */

#ifndef __ENCADAPTIVELOOPFILTER__
#define __ENCADAPTIVELOOPFILTER__

#include "CommonLib/AdaptiveLoopFilter.h"
#include "CommonLib/ParameterSetManager.h"

#include "CABACWriter.h"
#include "EncCfg.h"

struct AlfCovariance
{
  static constexpr int MAX_ALF_NUM_CLIP_VALS = AdaptiveLoopFilter::MAX_ALF_NUM_CLIP_VALS;
  using TE = double[MAX_NUM_ALF_LUMA_COEFF][MAX_NUM_ALF_LUMA_COEFF];
  using Ty = double[MAX_NUM_ALF_LUMA_COEFF];
  using TKE = TE[AdaptiveLoopFilter::MAX_ALF_NUM_CLIP_VALS][AdaptiveLoopFilter::MAX_ALF_NUM_CLIP_VALS];
  using TKy = Ty[AdaptiveLoopFilter::MAX_ALF_NUM_CLIP_VALS];

  int numCoeff;
  int numBins;
  TKy y;
  TKE E;
  double pixAcc;

  AlfCovariance() {}
  ~AlfCovariance() {}

  void create(int size, int _numBins = MAX_ALF_NUM_CLIP_VALS)
  {
    numCoeff = size;
    numBins  = _numBins;
    std::memset( y, 0, sizeof( y ) );
    std::memset( E, 0, sizeof( E ) );
  }

  void destroy()
  {
  }

  void reset(int _numBins = -1)
  {
    if (_numBins > 0)
    {
      numBins = _numBins;
    }
    pixAcc = 0;
    std::memset( y, 0, sizeof( y ) );
    std::memset( E, 0, sizeof( E ) );
  }

  const AlfCovariance& operator=( const AlfCovariance& src )
  {
    numCoeff = src.numCoeff;
    numBins = src.numBins;
    std::memcpy( E, src.E, sizeof( E ) );
    std::memcpy( y, src.y, sizeof( y ) );
    pixAcc = src.pixAcc;

    return *this;
  }

  void add( const AlfCovariance& lhs, const AlfCovariance& rhs )
  {
    numCoeff = lhs.numCoeff;
    numBins = lhs.numBins;
    for( int b0 = 0; b0 < numBins; b0++ )
    {
      for( int b1 = 0; b1 < numBins; b1++ )
      {
        for( int j = 0; j < numCoeff; j++ )
        {
          for( int i = 0; i < numCoeff; i++ )
          {
            E[b0][b1][j][i] = lhs.E[b0][b1][j][i] + rhs.E[b0][b1][j][i];
          }
        }
      }
    }
    for( int b = 0; b < numBins; b++ )
    {
      for( int j = 0; j < numCoeff; j++ )
      {
        y[b][j] = lhs.y[b][j] + rhs.y[b][j];
      }
    }
    pixAcc = lhs.pixAcc + rhs.pixAcc;
  }

  const AlfCovariance& operator+= ( const AlfCovariance& src )
  {
    for( int b0 = 0; b0 < numBins; b0++ )
    {
      for( int b1 = 0; b1 < numBins; b1++ )
      {
        for( int j = 0; j < numCoeff; j++ )
        {
          for( int i = 0; i < numCoeff; i++ )
          {
            E[b0][b1][j][i] += src.E[b0][b1][j][i];
          }
        }
      }
    }
    for( int b = 0; b < numBins; b++ )
    {
      for( int j = 0; j < numCoeff; j++ )
      {
        y[b][j] += src.y[b][j];
      }
    }
    pixAcc += src.pixAcc;

    return *this;
  }

  const AlfCovariance& operator-= ( const AlfCovariance& src )
  {
    for( int b0 = 0; b0 < numBins; b0++ )
    {
      for( int b1 = 0; b1 < numBins; b1++ )
      {
        for( int j = 0; j < numCoeff; j++ )
        {
          for( int i = 0; i < numCoeff; i++ )
          {
            E[b0][b1][j][i] -= src.E[b0][b1][j][i];
          }
        }
      }
    }
    for( int b = 0; b < numBins; b++ )
    {
      for( int j = 0; j < numCoeff; j++ )
      {
        y[b][j] -= src.y[b][j];
      }
    }
    pixAcc -= src.pixAcc;

    return *this;
  }

  void setEyFromClip(const int* clip, TE _E, Ty _y, int size) const
  {
    for (int k=0; k<size; k++)
    {
      _y[k] = y[clip[k]][k];
      for (int l=0; l<size; l++)
      {
        _E[k][l] = E[clip[k]][clip[l]][k][l];
      }
    }
  }

  double optimizeFilter(const int* clip, double *f, int size) const
  {
    gnsSolveByChol( clip, f, size );
    return calculateError( clip, f );
  }

  double optimizeFilter(const AlfFilterShape &alfShape, int *clip, double *f, bool optimizeClip) const;
  double optimizeFilterClip(const AlfFilterShape& alfShape, int* clip) const
  {
    Ty f;
    return optimizeFilter(alfShape, clip, f, true);
  }

  double calculateError( const int *clip ) const;
  double calculateError( const int *clip, const double *coeff ) const { return calculateError(clip, coeff, numCoeff); }
  double calculateError( const int *clip, const double *coeff, const int numCoeff ) const;
  double calcErrorForCoeffs(const int *clip, const int *coeff, const int numCoeff, const int fractionalBits) const;
  double calcErrorForCcAlfCoeffs(const int16_t *coeff, const int numCoeff, const int bitDepth) const;

  void getClipMax(const AlfFilterShape& alfShape, int *clip_max) const;
  void reduceClipCost(const AlfFilterShape& alfShape, int *clip) const;

  int  gnsSolveByChol( TE LHS, double* rhs, double *x, int numEq ) const;

private:
  // Cholesky decomposition

  int  gnsSolveByChol( const int *clip, double *x, int numEq ) const;
  void gnsBacksubstitution( TE R, double* z, int size, double* A ) const;
  void gnsTransposeBacksubstitution( TE U, double* rhs, double* x, int order ) const;
  int  gnsCholeskyDec( TE inpMatr, TE outMatr, int numEq ) const;
};

class EncAdaptiveLoopFilter : public AdaptiveLoopFilter
{
public:
  void setAlfWSSD(bool alfWSSD) { m_alfWSSD = alfWSSD; }
  void setLumaLevelWeightTable(const std::vector<double> &weightTable) { m_lumaLevelToWeightPLUT = weightTable; }

private:
  bool                m_alfWSSD{ false };
  std::vector<double> m_lumaLevelToWeightPLUT;

  const EncCfg*          m_encCfg;
  AlfCovariance***       m_alfCovariance[MAX_NUM_COMPONENT];          // [compIdx][shapeIdx][ctbAddr][classIdx]
  EnumArray<AlfCovariance **, ChannelType> m_alfCovarianceFrame;   // [CHANNEL][shapeIdx][lumaClassIdx/chromaAltIdx]
  AlfCovariance***       m_alfCovarianceCcAlf[2];           // [compIdx-1][shapeIdx][filterIdx][ctbAddr]
  AlfCovariance**        m_alfCovarianceFrameCcAlf[2];      // [compIdx-1][shapeIdx][filterIdx]

  //for RDO
  AlfParam               m_alfParamTemp;
  ParameterSetMap<APS>*  m_apsMap;
  AlfCovariance          m_alfCovarianceMerged[ALF_NUM_OF_FILTER_TYPES][MAX_NUM_ALF_CLASSES + 2];
  int                    m_alfClipMerged[ALF_NUM_OF_FILTER_TYPES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF];
  CABACWriter*           m_CABACEstimator;
  CtxPool               *m_ctxPool;
  double                 m_lambda[MAX_NUM_COMPONENT];

  int**                  m_filterCoeffSet; // [lumaClassIdx/chromaAltIdx][coeffIdx]
  int**                  m_filterClippSet; // [lumaClassIdx/chromaAltIdx][coeffIdx]
  int**                  m_diffFilterCoeff;
  short                  m_filterIndices[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES];

  EnumArray<unsigned, ChannelType> m_bitsNewFilter;

  int                    m_apsIdStart;
  double                 *m_ctbDistortionFixedFilter;
  double                 *m_ctbDistortionUnfilter[MAX_NUM_COMPONENT];
  std::vector<AlfMode>    m_indexTmpVec[MAX_NUM_COMPONENT];
  AlfMode                *m_indexTmp[MAX_NUM_COMPONENT];
  AlfParam               m_alfParamTempNL;
  int                    m_clipDefaultEnc[MAX_NUM_ALF_LUMA_COEFF];
  int                    m_filterTmp[MAX_NUM_ALF_LUMA_COEFF];
  int                    m_clipTmp[MAX_NUM_ALF_LUMA_COEFF];

  int m_apsIdCcAlfStart[2];

  short                  m_bestFilterCoeffSet[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF];
  bool                   m_bestFilterIdxEnabled[MAX_NUM_CC_ALF_FILTERS];
  uint8_t                m_bestFilterCount;
  uint8_t*               m_trainingCovControl;
  Pel*                   m_bufOrigin;
  PelBuf*                m_buf;
  uint64_t*              m_trainingDistortion[MAX_NUM_CC_ALF_FILTERS];    // for current block size
  uint64_t*              m_lumaSwingGreaterThanThresholdCount;
  uint64_t*              m_chromaSampleCountNearMidPoint;
  uint8_t*               m_filterControl;         // current iterations filter control
  uint8_t*               m_bestFilterControl;     // best saved filter control
  int                    m_reuseApsId[2];
  bool                   m_limitCcAlf;

public:
  EncAdaptiveLoopFilter();
  virtual ~EncAdaptiveLoopFilter() {}
  void  initDistortion();
  int   getAvailableApsIdsLuma(CodingStructure &cs);
  void  alfEncoderCtb(CodingStructure& cs, AlfParam& alfParamNewFilters
#if ENABLE_QPA
    , const double lambdaChromaWeight
#endif
  );
  void   alfReconstructor(CodingStructure& cs, const PelUnitBuf& recExtBuf);
  void ALFProcess(CodingStructure& cs, const double *lambdas
#if ENABLE_QPA
    , const double lambdaChromaWeight
#endif
    , Picture* pcPic, uint32_t numSliceSegments
  );
  int getNewCcAlfApsId(CodingStructure &cs, int cIdx);
  void initCABACEstimator(CABACEncoder *cabacEncoder, CtxPool *ctxPool, Slice *pcSlice, ParameterSetMap<APS> *apsMap);
  void create(const EncCfg* encCfg, const int picWidth, const int picHeight, const ChromaFormat chromaFormatIdc,
              const int maxCUWidth, const int maxCUHeight, const int maxCUDepth, const BitDepths& inputBitDepth,
              const BitDepths& internalBitDepth);
  void destroy();
  void setApsIdStart( int i) { m_apsIdStart = i; }

private:
  void firstPass(CodingStructure &cs, AlfParam &alfParam, const PelUnitBuf &orgUnitBuf, const PelUnitBuf &recExtBuf,
                 const PelUnitBuf &recBuf, const ChannelType channel
#if ENABLE_QPA
                 ,
                 const double lambdaChromaWeight = 0.0
#endif
  );

  void   copyAlfParam( AlfParam& alfParamDst, AlfParam& alfParamSrc, ChannelType channel );
  double mergeFiltersAndCost(AlfParam &alfParam, AlfFilterShape &alfShape, AlfCovariance *covFrame,
                             AlfCovariance *covMerged,
                             int  clipMerged[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF],
                             int &coeffBitsFinal);

  void   getFrameStats(ChannelType channel, int shapeIdx);
  void   getFrameStat(AlfCovariance *frameCov, AlfCovariance **ctbCov, AlfMode *ctbEnableFlags, const int numClasses,
                      int altIdx);
  void   deriveStatsForFiltering( PelUnitBuf& orgYuv, PelUnitBuf& recYuv, CodingStructure& cs );
  void   getBlkStats(AlfCovariance *alfCovariace, const AlfFilterShape &shape, AlfClassifier **classifier, Pel *org,
                     const ptrdiff_t orgStride, const Pel *orgLuma, const ptrdiff_t orgLumaStride, Pel *rec,
                     const ptrdiff_t recStride, const CompArea &areaDst, const CompArea &area, const ChannelType channel,
                     int vbCTUHeight, int vbPos);
  void calcCovariance(Pel ELocal[MAX_NUM_ALF_LUMA_COEFF][MAX_ALF_NUM_CLIP_VALS], const Pel *rec, const ptrdiff_t stride,
                      const AlfFilterShape &shape, const int transposeIdx, const ChannelType channel, int vbDistance);
  void   deriveStatsForCcAlfFiltering(const PelUnitBuf &orgYuv, const PelUnitBuf &recYuv, const int compIdx,
                                      const int maskStride, const uint8_t filterIdc, CodingStructure &cs);
  void   getBlkStatsCcAlf(AlfCovariance &alfCovariance, const AlfFilterShape &shape, const PelUnitBuf &orgYuv,
                          const PelUnitBuf &recYuv, const UnitArea &areaDst, const UnitArea &area,
                          const ComponentID compID, const int yPos);
  void   calcCovarianceCcAlf(Pel ELocal[MAX_NUM_CC_ALF_CHROMA_COEFF][1], const Pel *rec, const ptrdiff_t stride,
                             const AlfFilterShape &shape, int vbDistance);
  void   mergeClasses(const AlfFilterShape& alfShape, AlfCovariance* cov, AlfCovariance* covMerged, int clipMerged[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF], const int numClasses, short filterIndices[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES]);

  double getFilterCoeffAndCost(CodingStructure &cs, double distUnfilter, ChannelType channel, bool bReCollectStat,
                               int shapeIdx, int &coeffBits, bool onlyFilterCost = false);
  double deriveFilterCoeffs(AlfCovariance* cov, AlfCovariance* covMerged, int clipMerged[MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_CLASSES][MAX_NUM_ALF_LUMA_COEFF], AlfFilterShape& alfShape, short* filterIndices, int numFilters, double errorTabForce0Coeff[MAX_NUM_ALF_CLASSES][2], AlfParam& alfParam);
  int    deriveFilterCoefficientsPredictionMode( AlfFilterShape& alfShape, int **filterSet, int** filterCoeffDiff, const int numFilters );
  double deriveCoeffQuant(int *filterClipp, int *filterCoeffQuant, const AlfCovariance &cov,
                          const AlfFilterShape &shape, const int fractionalBits, const bool optimizeClip);
  double deriveCtbAlfEnableFlags(CodingStructure &cs, const int shapeIdx, ChannelType channel,
#if ENABLE_QPA
                                 const double chromaWeight,
#endif
                                 const int numClasses, const int numCoeff, double &distUnfilter);
  void   roundFiltCoeff( int *filterCoeffQuant, double *filterCoeff, const int numCoeff, const int factor );
  void   roundFiltCoeffCCALF(int16_t *filterCoeffQuant, double *filterCoeff, const int numCoeff, int factor);

  double getDistCoeffForce0( bool* codedVarBins, double errorForce0CoeffTab[MAX_NUM_ALF_CLASSES][2], int* bitsVarBin, int zeroBitsVarBin, const int numFilters);
  int    lengthUvlc( int uiCode );
  int    getNonFilterCoeffRate( AlfParam& alfParam );

  int    getCostFilterCoeffForce0( AlfFilterShape& alfShape, int **pDiffQFilterCoeffIntPP, const int numFilters, bool* codedVarBins );
  int    getCostFilterCoeff( AlfFilterShape& alfShape, int **pDiffQFilterCoeffIntPP, const int numFilters );
  int    getCostFilterClipp( AlfFilterShape& alfShape, int **pDiffQFilterCoeffIntPP, const int numFilters );
  int    lengthFilterCoeffs( AlfFilterShape& alfShape, const int numFilters, int **FilterCoeff );
  double getDistForce0( AlfFilterShape& alfShape, const int numFilters, double errorTabForce0Coeff[MAX_NUM_ALF_CLASSES][2], bool* codedVarBins );
  int    getChromaCoeffRate( AlfParam& alfParam, int altIdx );

  double getUnfilteredDistortion( AlfCovariance* cov, ChannelType channel );
  double getUnfilteredDistortion( AlfCovariance* cov, const int numClasses );
  double getFilteredDistortion( AlfCovariance* cov, const int numClasses, const int numFiltersMinus1, const int numCoeff );

  void setSliceEnabledFlag(AlfParam &alfSlicePara, ChannelType channel, bool val);
  void setSliceEnabledFlag(AlfParam &alfSlicePara, ChannelType channel, AlfMode *ctuFlags[MAX_NUM_COMPONENT]);
  void setCtuEnableFlag(AlfMode *ctuFlags[MAX_NUM_COMPONENT], ChannelType channel, AlfMode val);
  void copyIndices(AlfMode *ctuFlagsDst[MAX_NUM_COMPONENT], AlfMode *ctuFlagsSrc[MAX_NUM_COMPONENT],
                   ChannelType channel);
  void initCtuAlternativeChroma(AlfMode *ctuAlts[MAX_NUM_COMPONENT]);
  int getMaxNumAlternativesChroma( );
  int  getCoeffRateCcAlf(short chromaCoeff[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF], bool filterEnabled[MAX_NUM_CC_ALF_FILTERS], uint8_t filterCount, ComponentID compID);
  void deriveCcAlfFilterCoeff( ComponentID compID, const PelUnitBuf& recYuv, const PelUnitBuf& recYuvExt, short filterCoeff[MAX_NUM_CC_ALF_FILTERS][MAX_NUM_CC_ALF_CHROMA_COEFF], const uint8_t filterIdx );
  void determineControlIdcValues(CodingStructure &cs, const ComponentID compID, const PelBuf *buf, const int ctuWidthC,
                                 const int ctuHeightC, const int picWidthC, const int picHeightC,
                                 double **unfilteredDistortion, uint64_t *trainingDistortion[MAX_NUM_CC_ALF_FILTERS],
                                 uint64_t *lumaSwingGreaterThanThresholdCount,
                                 uint64_t *chromaSampleCountNearMidPoint,
                                 bool reuseFilterCoeff, uint8_t *trainingCovControl, uint8_t *filterControl,
                                 uint64_t &curTotalDistortion, double &curTotalRate,
                                 bool     filterEnabled[MAX_NUM_CC_ALF_FILTERS],
                                 uint8_t  mapFilterIdxToFilterIdc[MAX_NUM_CC_ALF_FILTERS + 1],
                                 uint8_t &ccAlfFilterCount);
  void deriveCcAlfFilter( CodingStructure& cs, ComponentID compID, const PelUnitBuf& orgYuv, const PelUnitBuf& tempDecYuvBuf, const PelUnitBuf& dstYuv );
  std::vector<int> getAvailableCcAlfApsIds(CodingStructure& cs, ComponentID compID);
  void xSetupCcAlfAPS( CodingStructure& cs );
  void             countLumaSwingGreaterThanThreshold(const Pel *luma, ptrdiff_t lumaStride, int height, int width,
                                                      int log2BlockWidth, int log2BlockHeight,
                                                      uint64_t *lumaSwingGreaterThanThresholdCount, int lumaCountStride);
  void             countChromaSampleValueNearMidPoint(const Pel *chroma, ptrdiff_t chromaStride, int height, int width,
                                                      int log2BlockWidth, int log2BlockHeight,
                                                      uint64_t *chromaSampleCountNearMidPoint,
                                                      int       chromaSampleCountNearMidPointStride);
  void getFrameStatsCcalf(ComponentID compIdx, int filterIdc);
  void initDistortionCcalf();
};



#endif
