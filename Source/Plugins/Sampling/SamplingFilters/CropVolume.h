/* ============================================================================
 * Copyright (c) 2011 Michael A. Jackson (BlueQuartz Software)
 * Copyright (c) 2011 Dr. Michael A. Groeber (US Air Force Research Laboratories)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of Michael A. Groeber, Michael A. Jackson, the US Air Force,
 * BlueQuartz Software nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  This code was written under United States Air Force Contract number
 *                           FA8650-07-D-5800
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#ifndef CROPVOLUME_H_
#define CROPVOLUME_H_

#include <QtCore/QString>

#include "DREAM3DLib/DREAM3DLib.h"
#include "DREAM3DLib/Common/DREAM3DSetGetMacros.h"
#include "DREAM3DLib/DataArrays/IDataArray.h"

#include "DREAM3DLib/Common/AbstractFilter.h"
#include "DREAM3DLib/DataContainers/VolumeDataContainer.h"

/**
 * @class CropVolume CropVolume.h DREAM3DLib/SyntheticBuilderFilters/CropVolume.h
 * @brief
 * @author
 * @date Nov 19, 2011
 * @version 1.0
 */
class CropVolume : public AbstractFilter
{
    Q_OBJECT /* Need this for Qt's signals and slots mechanism to work */
  public:
    DREAM3D_SHARED_POINTERS(CropVolume)
    DREAM3D_STATIC_NEW_MACRO(CropVolume)
    DREAM3D_TYPE_MACRO_SUPER(CropVolume, AbstractFilter)

    virtual ~CropVolume();
    DREAM3D_INSTANCE_STRING_PROPERTY(DataContainerName)
    DREAM3D_INSTANCE_STRING_PROPERTY(NewDataContainerName)
    DREAM3D_INSTANCE_STRING_PROPERTY(CellFeatureAttributeMatrixName)
    DREAM3D_INSTANCE_STRING_PROPERTY(CellAttributeMatrixName)

    DREAM3D_FILTER_PARAMETER(int, XMin)
    Q_PROPERTY(int XMin READ getXMin WRITE setXMin NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(int, YMin)
    Q_PROPERTY(int YMin READ getYMin WRITE setYMin NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(int, ZMin)
    Q_PROPERTY(int ZMin READ getZMin WRITE setZMin NOTIFY parametersChanged)

    DREAM3D_FILTER_PARAMETER(int, XMax)
    Q_PROPERTY(int XMax READ getXMax WRITE setXMax NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(int, YMax)
    Q_PROPERTY(int YMax READ getYMax WRITE setYMax NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(int, ZMax)
    Q_PROPERTY(int ZMax READ getZMax WRITE setZMax NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(bool, RenumberFeatures)
    Q_PROPERTY(bool RenumberFeatures READ getRenumberFeatures WRITE setRenumberFeatures NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(bool, SaveAsNewDataContainer)
    Q_PROPERTY(bool SaveAsNewDataContainer READ getSaveAsNewDataContainer WRITE setSaveAsNewDataContainer NOTIFY parametersChanged)
    DREAM3D_FILTER_PARAMETER(bool, UpdateOrigin)
    Q_PROPERTY(bool UpdateOrigin READ getUpdateOrigin WRITE setUpdateOrigin NOTIFY parametersChanged)


    virtual const QString getGroupName() { return DREAM3D::FilterGroups::SamplingFilters; }
    virtual const QString getSubGroupName()  { return DREAM3D::FilterSubGroups::CropCutFilters; }
    virtual const QString getHumanLabel() { return "Crop Volume"; }

    virtual void setupFilterParameters();
    /**
    * @brief This method will write the options to a file
    * @param writer The writer that is used to write the options to a file
    */
    virtual int writeFilterParameters(AbstractFilterParametersWriter* writer, int index);

    /**
    * @brief This method will read the options from a file
    * @param reader The reader that is used to read the options from a file
    */
    virtual void readFilterParameters(AbstractFilterParametersReader* reader, int index);

    /**
       * @brief Reimplemented from @see AbstractFilter class
       */
    virtual void execute();
    virtual void preflight();

  signals:
    void parametersChanged();
    void preflightAboutToExecute();
    void preflightExecuted();

  protected:
    CropVolume();


  private:
    DEFINE_PTR_WEAKPTR_DATAARRAY(int32_t, FeatureIds)

    void dataCheck();
    void updateCellInstancePointers();

    CropVolume(const CropVolume&); // Copy Constructor Not Implemented
    void operator=(const CropVolume&); // Operator '=' Not Implemented
};

#endif /* CROPVOLUME_H_ */





