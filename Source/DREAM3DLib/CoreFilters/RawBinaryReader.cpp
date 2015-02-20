/* ============================================================================
 * Copyright (c) 2012 Michael A. Jackson (BlueQuartz Software)
 * Copyright (c) 2012 Dr. Michael A. Groeber (US Air Force Research Laboratories)
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
#include "RawBinaryReader.h"

#include <stdio.h>



#include <QtCore/QFileInfo>
#include <QtCore/QFile>

#include "DREAM3DLib/Common/ScopedFileMonitor.hpp"
#include "DREAM3DLib/Utilities/DREAM3DEndian.h"


#define RBR_FILE_NOT_OPEN -1000
#define RBR_FILE_TOO_SMALL -1010
#define RBR_FILE_TOO_BIG -1020
#define RBR_READ_EOF       -1030
#define RBR_NO_ERROR       0


namespace Detail
{
  enum NumType
  {
    Int8 = 0,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    UnknownNumType
  };
}

#ifdef CMP_WORDS_BIGENDIAN
#define SWAP_ARRAY(array)\
  if (m_Endian == 0) { array->byteSwapElements(); }

#else
#define SWAP_ARRAY(array)\
  if (m_Endian == 1) { array->byteSwapElements(); }

#endif


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SanityCheckFileSizeVersusAllocatedSize(size_t allocatedBytes, size_t fileSize, int skipHeaderBytes)
{
  if (fileSize - skipHeaderBytes < allocatedBytes)
  {
    return -1;
  }
  else if (fileSize - skipHeaderBytes > allocatedBytes)
  {
    return 1;
  }
  // File Size and Allocated Size are equal so we  are good to go
  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
template<typename T>
int readBinaryFile(typename DataArray<T>::Pointer p, const QString& filename, int skipHeaderBytes)
{
  int err = 0;
  QFileInfo fi(filename);
  uint64_t fileSize = fi.size();
  size_t allocatedBytes = p->getSize() * sizeof(T);
  err = SanityCheckFileSizeVersusAllocatedSize(allocatedBytes, fileSize, skipHeaderBytes);

  if (err < 0)
  {
    return RBR_FILE_TOO_SMALL;
  }

  FILE* f = fopen(filename.toLatin1().data(), "rb");
  if (NULL == f)
  {
    return RBR_FILE_NOT_OPEN;
  }

  ScopedFileMonitor monitor(f);
  size_t numBytesToRead = p->getNumberOfTuples() * static_cast<size_t>(p->getNumberOfComponents()) * sizeof(T);
  size_t numRead = 0;

  unsigned char* chunkptr = reinterpret_cast<unsigned char*>(p->getPointer(0));

  //Skip some header bytes by just reading those bytes into the pointer knowing that the next
  // thing we are going to do it over write those bytes with the real data that we are after.
  if (skipHeaderBytes > 0)
  {
    numRead = fread(chunkptr, 1, skipHeaderBytes, f);
  }
  numRead = 0;
  // Now start reading the data in chunks if needed.
  size_t chunkSize = DEFAULT_BLOCKSIZE;

  if(numBytesToRead < DEFAULT_BLOCKSIZE)
  {
    chunkSize = numBytesToRead;
  }

  size_t master_counter = 0;
  size_t bytes_read = 0;
  while(1)
  {
    bytes_read = fread(chunkptr, sizeof(unsigned char), chunkSize, f);
    chunkptr = chunkptr + bytes_read;
    master_counter += bytes_read;

    if( numBytesToRead - master_counter < chunkSize)
    {
      chunkSize = numBytesToRead - master_counter;
    }
    if(master_counter >= numBytesToRead)
    {
      break;
    }

  }

  return RBR_NO_ERROR;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
RawBinaryReader::RawBinaryReader() :
  AbstractFilter(),
  m_NewDataContainerName(DREAM3D::Defaults::DataContainerName),
  m_ExistingDataContainerName(DREAM3D::Defaults::DataContainerName),
  m_AttributeMatrixPath(DREAM3D::Defaults::DataContainerName, DREAM3D::Defaults::CellAttributeMatrixName, ""),
  m_CellAttributeMatrixName(DREAM3D::Defaults::CellAttributeMatrixName),
  m_CellFeatureAttributeMatrixName(DREAM3D::Defaults::CellFeatureAttributeMatrixName),
  m_CellEnsembleAttributeMatrixName(DREAM3D::Defaults::CellEnsembleAttributeMatrixName),
  m_ScalarType(0),
  m_Endian(0),
  m_NumberOfComponents(0),
  m_NumFeatures(0),
  m_NumPhases(0),
  m_AttributeMatrixType(0),
  m_SkipHeaderBytes(0),
  m_OutputArrayName(""),
  m_InputFile(""),
  m_AddToExistingDataContainer(false),
  m_AddToExistingAttributeMatrix(false)
{
  m_Dimensions.x = 0;
  m_Dimensions.y = 0;
  m_Dimensions.z = 0;

  m_Origin.x = 0.0;
  m_Origin.y = 0.0;
  m_Origin.z = 0.0;

  m_Resolution.x = 1.0;
  m_Resolution.y = 1.0;
  m_Resolution.z = 1.0;
  setupFilterParameters();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
RawBinaryReader::~RawBinaryReader()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void RawBinaryReader::setupFilterParameters()
{
  FilterParameterVector parameters;
  /* Place all your option initialization code here */

  //information about binary file
  parameters.push_back(FilterParameter::New("File Information", "", FilterParameterWidgetType::SeparatorWidget, "", false));
  parameters.push_back(FileSystemFilterParameter::New("Input File", "InputFile", FilterParameterWidgetType::InputFileWidget, getInputFile(), false, "", "*.raw *.bin"));
  {
    ChoiceFilterParameter::Pointer parameter = ChoiceFilterParameter::New();
    parameter->setHumanLabel("Scalar Type");
    parameter->setPropertyName("ScalarType");
    parameter->setWidgetType(FilterParameterWidgetType::ChoiceWidget);
    QVector<QString> choices;
    choices.push_back("signed   int 8  bit");
    choices.push_back("unsigned int 8  bit");
    choices.push_back("signed   int 16 bit");
    choices.push_back("unsigned int 16 bit");
    choices.push_back("signed   int 32 bit");
    choices.push_back("unsigned int 32 bit");
    choices.push_back("signed   int 64 bit");
    choices.push_back("unsigned int 64 bit");
    choices.push_back("       Float 32 bit");
    choices.push_back("      Double 64 bit");
    parameter->setChoices(choices);
    parameters.push_back(parameter);
  }
  parameters.push_back(FilterParameter::New("Number Of Components", "NumberOfComponents", FilterParameterWidgetType::IntWidget, getNumberOfComponents(), false));
  {
    ChoiceFilterParameter::Pointer parameter = ChoiceFilterParameter::New();
    parameter->setHumanLabel("Endian");
    parameter->setPropertyName("Endian");
    parameter->setWidgetType(FilterParameterWidgetType::ChoiceWidget);
    QVector<QString> choices;
    choices.push_back("Little");
    choices.push_back("Big");
    parameter->setChoices(choices);
    parameters.push_back(parameter);
  }
  parameters.push_back(FilterParameter::New("Skip Header Bytes", "SkipHeaderBytes", FilterParameterWidgetType::IntWidget, getSkipHeaderBytes(), false));

  

  //existing or new datacontainer
  parameters.push_back(FilterParameter::New("Data Container Information", "", FilterParameterWidgetType::SeparatorWidget, "", false));
  {
    QStringList linkedProps;
    linkedProps << "ExistingDataContainerName" << "AddToExistingAttributeMatrix";
    parameters.push_back(LinkedBooleanFilterParameter::New("Add to Existing DataContainer", "AddToExistingDataContainer", getAddToExistingDataContainer(), linkedProps, false));
  }
  //to be least confusing this should appear only when AddToExistingDataContainer && !AddToExistingAttributeMatrix
  parameters.push_back(FilterParameter::New("Data Container", "ExistingDataContainerName", FilterParameterWidgetType::DataContainerSelectionWidget, getExistingDataContainerName(), false, ""));  

  //to be least confusing these should appear only when !AddToExistingDataContainer
  parameters.push_back(FilterParameter::New("Dimensions", "Dimensions", FilterParameterWidgetType::IntVec3Widget, getDimensions(), false, "XYZ", 0));
  parameters.push_back(FilterParameter::New("Origin", "Origin", FilterParameterWidgetType::FloatVec3Widget, getOrigin(), false, "XYZ"));
  parameters.push_back(FilterParameter::New("Resolution", "Resolution", FilterParameterWidgetType::FloatVec3Widget, getResolution(), false, "XYZ"));



  //exiting or new attribute matrix
  {
    QStringList linkedProps;
    linkedProps << "AttributeMatrixPath";
    parameters.push_back(LinkedBooleanFilterParameter::New("Add to Existing AttributeMatrix", "AddToExistingAttributeMatrix", getAddToExistingAttributeMatrix(), linkedProps, false));
  }
  parameters.push_back(FilterParameter::New("Attribute Matrix", "AttributeMatrixPath", FilterParameterWidgetType::AttributeMatrixSelectionWidget, getAttributeMatrixPath(), false, ""));
  
  //to be least confusing this comobobox (and all linked parameters) should only appear if !AddToExistingAttributeMatrix
  {
    LinkedChoicesFilterParameter::Pointer parameter = LinkedChoicesFilterParameter::New();
    parameter->setHumanLabel("Attribute Matrix Type");
    parameter->setPropertyName("AttributeMatrixType");
    parameter->setWidgetType(FilterParameterWidgetType::ChoiceWidget);
    parameter->setDefaultValue(getAttributeMatrixType()); // Just set the first index

    QVector<QString> choices;
    choices.push_back("Cell");
    choices.push_back("Cell Feature");
    choices.push_back("Cell Ensemble");
    parameter->setChoices(choices);
    QStringList linkedProps;
    linkedProps << "NumFeatures" << "NumPhases" << "CellAttributeMatrixName" << "CellFeatureAttributeMatrixName" << "CellEnsembleAttributeMatrixName";
    parameter->setLinkedProperties(linkedProps);
    parameter->setEditable(false);
    parameters.push_back(parameter);

  }
  //atribute matrix dimensions (type specific)
  parameters.push_back(FilterParameter::New("Number of Features", "NumFeatures", FilterParameterWidgetType::IntWidget, getNumFeatures(), false, "", 1));
  parameters.push_back(FilterParameter::New("Number of Phases", "NumPhases", FilterParameterWidgetType::IntWidget, getNumPhases(), false, "", 2));


  //names for created information
  parameters.push_back(FilterParameter::New("Created Information", "", FilterParameterWidgetType::SeparatorWidget, "", false));
  parameters.push_back(FilterParameter::New("Output Array Name", "OutputArrayName", FilterParameterWidgetType::StringWidget, getOutputArrayName(), false));

  //to be least confusing this should only appear when !AddToExistingDataContainer
  parameters.push_back(FilterParameter::New("New Data Container Name", "NewDataContainerName", FilterParameterWidgetType::StringWidget, getNewDataContainerName(), true));

  //different default names for different attribute matrix types (to be least confusing these should only appear when !AddToExistingAttributeMatrix (in addition to appropriate combobox selection))
  parameters.push_back(FilterParameter::New("Cell Attribute Matrix Name", "CellAttributeMatrixName", FilterParameterWidgetType::StringWidget, getCellAttributeMatrixName(), true, "", 0));
  parameters.push_back(FilterParameter::New("Cell Feature Attribute Matrix Name", "CellFeatureAttributeMatrixName", FilterParameterWidgetType::StringWidget, getCellFeatureAttributeMatrixName(), true, "", 1));
  parameters.push_back(FilterParameter::New("Cell Ensemble Attribute Matrix Name", "CellEnsembleAttributeMatrixName", FilterParameterWidgetType::StringWidget, getCellEnsembleAttributeMatrixName(), true, "", 2));
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void RawBinaryReader::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setInputFile( reader->readString( "InputFile", getInputFile() ) );
  setScalarType( reader->readValue("ScalarType", getScalarType()) );
  setNumberOfComponents( reader->readValue("NumberOfComponents", getNumberOfComponents()) );
  setEndian( reader->readValue("Endian", getEndian()) );
  setSkipHeaderBytes( reader->readValue("SkipHeaderBytes", getSkipHeaderBytes()) );

  setAddToExistingDataContainer(reader->readValue("AddToExistingDataContainer", getAddToExistingDataContainer() ) );
  setExistingDataContainerName(reader->readString("ExistingDataContainerName", getExistingDataContainerName() ) );
  setOrigin( reader->readFloatVec3("Origin", getOrigin() ) );
  setResolution( reader->readFloatVec3("Resolution", getResolution() ) );

  setAddToExistingAttributeMatrix(reader->readValue("AddToExistingAttributeMatrix", getAddToExistingAttributeMatrix() ) );
  setAttributeMatrixPath(reader->readDataArrayPath("AttributeMatrixPath", getAttributeMatrixPath() ) );
  setAttributeMatrixType(reader->readValue("AttributeMatrixType", getAttributeMatrixType() ) );
  setDimensions( reader->readIntVec3("Dimensions", getDimensions() ) );
  setNumFeatures(reader->readValue("NumFeatures", getNumFeatures() ) );
  setNumPhases(reader->readValue("NumPhases", getNumPhases() ) );

  setOutputArrayName( reader->readString( "OutputArrayName", getOutputArrayName() ) );
  setNewDataContainerName(reader->readString("NewDataContainerName", getNewDataContainerName() ) );
  setCellAttributeMatrixName(reader->readString("CellAttributeMatrixName", getCellAttributeMatrixName() ) );
  setCellFeatureAttributeMatrixName(reader->readString("CellFeatureAttributeMatrixName", getCellFeatureAttributeMatrixName() ) );
  setCellEnsembleAttributeMatrixName(reader->readString("CellEnsembleAttributeMatrixName", getCellEnsembleAttributeMatrixName() ) );
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int RawBinaryReader::writeFilterParameters(AbstractFilterParametersWriter* writer, int index)
{
  writer->openFilterGroup(this, index);
  DREAM3D_FILTER_WRITE_PARAMETER(InputFile)
  DREAM3D_FILTER_WRITE_PARAMETER(ScalarType)
  DREAM3D_FILTER_WRITE_PARAMETER(NumberOfComponents)
  DREAM3D_FILTER_WRITE_PARAMETER(Endian)
  DREAM3D_FILTER_WRITE_PARAMETER(SkipHeaderBytes)

  DREAM3D_FILTER_WRITE_PARAMETER(AddToExistingDataContainer)
  DREAM3D_FILTER_WRITE_PARAMETER(ExistingDataContainerName)
  DREAM3D_FILTER_WRITE_PARAMETER(Origin)
  DREAM3D_FILTER_WRITE_PARAMETER(Resolution)

  DREAM3D_FILTER_WRITE_PARAMETER(AddToExistingAttributeMatrix)
  DREAM3D_FILTER_WRITE_PARAMETER(AttributeMatrixPath)
  DREAM3D_FILTER_WRITE_PARAMETER(AttributeMatrixType)
  DREAM3D_FILTER_WRITE_PARAMETER(Dimensions)
  DREAM3D_FILTER_WRITE_PARAMETER(NumFeatures)
  DREAM3D_FILTER_WRITE_PARAMETER(NumPhases)
  
  DREAM3D_FILTER_WRITE_PARAMETER(OutputArrayName)
  DREAM3D_FILTER_WRITE_PARAMETER(NewDataContainerName)
  DREAM3D_FILTER_WRITE_PARAMETER(CellAttributeMatrixName)
  DREAM3D_FILTER_WRITE_PARAMETER(CellFeatureAttributeMatrixName)
  DREAM3D_FILTER_WRITE_PARAMETER(CellEnsembleAttributeMatrixName)
  writer->closeFilterGroup();
  return ++index; // we want to return the next index that was just written to
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void RawBinaryReader::dataCheck(bool preflight)
{
  setErrorCondition(0);

  //get file name and make sure it exists
  QFileInfo fi(getInputFile());
  if (getInputFile().isEmpty() == true)
  {
    QString ss = QObject::tr("%1 needs the Input File Set and it was not.").arg(ClassName());
    setErrorCondition(-387);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
  }
  else if (fi.exists() == false)
  {
    QString ss = QObject::tr("The input file does not exist");
    setErrorCondition(-388);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
  }

  //make sure a name has been selected for the created data
  if(m_OutputArrayName.isEmpty() == true)
  {
    QString ss = QObject::tr("The Output Array Name is blank (empty) and a value must be filled in for the pipeline to complete.");
    setErrorCondition(-398);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
  }

  //make sure there is at least 1 component in 1 dimension
  if (m_NumberOfComponents < 1)
  {
    QString ss = QObject::tr("The number of components must be larger than Zero");
    setErrorCondition(-391);
    notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
  }

  //check dimensions if needed (new data contianer)
  if( !getAddToExistingDataContainer() ) 
  {
    if (  m_Dimensions.x == 0 || m_Dimensions.y == 0 || m_Dimensions.z == 0)
    {
      QString ss = QObject::tr("One of the dimensions has a size less than or Equal to Zero (0). The minimum size must be greater than One (1).");
      setErrorCondition(-390);
      notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    }
  }

  DataContainer::Pointer m = DataContainer::NullPointer();
  AttributeMatrix::Pointer attrMat;

  //get or create data container
  if(getAddToExistingDataContainer())
  {
    //existing data container, get name from correct place (depends on if we will also use existing attribute matrix)
    if(getAddToExistingAttributeMatrix())
    {
      //existing data continaer and attirbute matrix
      m = getDataContainerArray()->getPrereqDataContainer<AbstractFilter>(this, getAttributeMatrixPath().getDataContainerName());
    }
    else
    {
      //existing data continaer but new attirbute matrix
      m = getDataContainerArray()->getPrereqDataContainer<AbstractFilter>(this, getExistingDataContainerName());
    }
    if(getErrorCondition() < 0)
    {
      return;
    }
  }
  else
  {
    //new data container
    m = getDataContainerArray()->createNonPrereqDataContainer<AbstractFilter>(this, getNewDataContainerName());
    if(getErrorCondition() < 0)
    {
      return;
    }

    //if creating new data container, set geometry
    if(!getAddToExistingDataContainer())
    {
      ImageGeom::Pointer image = ImageGeom::CreateGeometry("BinaryImage");
      image->setDimensions(m_Dimensions.x, m_Dimensions.y, m_Dimensions.z);
      image->setResolution(m_Resolution.x, m_Resolution.y, m_Resolution.z);
      image->setOrigin(m_Origin.x, m_Origin.y, m_Origin.z);
      m->setGeometry(image);
    }
    //otherwise make sure it is the correct type
    else if(DREAM3D::GeometryType::ImageGeometry != m->getGeometry()->getGeometryType())
    {
      QString ss = QObject::tr("Rectilinear grid geometry required.");
      setErrorCondition(-390);
      notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
      return;
    }

  }

  //get or create attribute matrix
  if(getAddToExistingDataContainer() && getAddToExistingAttributeMatrix())
  {
    //existing attribute matrix
    attrMat = m->getPrereqAttributeMatrix<AbstractFilter>(this, getAttributeMatrixPath().getAttributeMatrixName(), -10000);
    if(getErrorCondition() < 0)
    {
      return;
    }

    //set attribute matrix type
    switch(attrMat->getType())
    {
      case DREAM3D::AttributeMatrixType::Cell:
      {
        setAttributeMatrixType(0);
      }
      break;

      case DREAM3D::AttributeMatrixType::CellFeature:
      {
        setAttributeMatrixType(1);
      }
      break;

      case DREAM3D::AttributeMatrixType::CellEnsemble:
      {
        setAttributeMatrixType(2);
      }
      break;

      default:
      {
      QString ss = QObject::tr("Selected AttributeMatrix has unsupported type.");
      setErrorCondition(-391);
      notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
      return;
      }
    }
  }
  else
  {
    //new attribute matrix (may be new data container + attribute matrix or just new data container, may not)
    switch(getAttributeMatrixType())
    {
      //cell
      case 0:
        {
          //use dimensions of created or existing data container
          QVector<size_t> tDims(3, 0);
          ImageGeom::Pointer igeom = m->getGeometryAs<ImageGeom>();
          tDims[0] = igeom->getXPoints();
          tDims[1] = igeom->getYPoints();
          tDims[2] = igeom->getZPoints();          
          attrMat = m->createNonPrereqAttributeMatrix<AbstractFilter>(this, getCellAttributeMatrixName(), tDims, DREAM3D::AttributeMatrixType::Cell);
        }
        break;

      //cell feature
      case 1:
        {
          QVector<size_t> tDims(1, getNumFeatures() + 1);//1 spot for each feature + feature 0
          attrMat = m->createNonPrereqAttributeMatrix<AbstractFilter>(this, getCellFeatureAttributeMatrixName(), tDims, DREAM3D::AttributeMatrixType::CellFeature);
        }
        break;


      //cell ensemble
      case 2:
        {
          QVector<size_t> tDims(1, getNumPhases() + 1);//1 spot for each phase + bad phase
          attrMat = m->createNonPrereqAttributeMatrix<AbstractFilter>(this, getCellEnsembleAttributeMatrixName(), tDims, DREAM3D::AttributeMatrixType::CellEnsemble);
        }
        break;

      default:
        //future location for other types
        QString ss = QObject::tr("Unsupported AttributeMatrix type selected.");
        setErrorCondition(-1);
        notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    }
    if(getErrorCondition() < 0)
    {
      return;
    }
  }

  //create array for preflight
  if (true == preflight)
  {
    IDataArray::Pointer p = IDataArray::NullPointer();
    QVector<size_t> dims(1, m_NumberOfComponents);

    //determine number of elements
    QVector<size_t> matDims = attrMat->getTupleDimensions();
    size_t allocatedBytes = m_NumberOfComponents;
    for(size_t i = 0; i < matDims.size(); i++)
    {
      allocatedBytes *= matDims[i];
    }

    // switch(getAttributeMatrixType())
    // {
    //   //cell
    //   case 0:
    //     {
    //       allocatedBytes = m_NumberOfComponents * m_Dimensions.x * m_Dimensions.y * m_Dimensions.z;
    //     }
    //     break;

    //   //cell feature
    //   case 1:
    //     {
    //       allocatedBytes = m_NumberOfComponents * getNumFeatures();
    //     }
    //     break;


    //   //cell ensemble
    //   case 2:
    //     {
    //       allocatedBytes = m_NumberOfComponents * getNumPhases();
    //     }
    //     break;

    //   default:
    //     //future location for other types
    //     QString ss = QObject::tr("Unsupported AttributeMatrix type selected.");
    //     setErrorCondition(-1);
    //     notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    //     return;
    // }

    //create correctly typed array + scale number of bytes with type
    if (m_ScalarType == Detail::Int8)
    {
      attrMat->createAndAddAttributeArray<DataArray<int8_t>, AbstractFilter, int8_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(int8_t);
    }
    else if (m_ScalarType == Detail::UInt8)
    {
      attrMat->createAndAddAttributeArray<DataArray<uint8_t>, AbstractFilter, uint8_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(uint8_t);
    }
    else if (m_ScalarType == Detail::Int16)
    {
      attrMat->createAndAddAttributeArray<DataArray<int16_t>, AbstractFilter, int16_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(int16_t);
    }
    else if (m_ScalarType == Detail::UInt16)
    {
      attrMat->createAndAddAttributeArray<DataArray<uint16_t>, AbstractFilter, uint16_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(uint16_t);
    }
    else if (m_ScalarType == Detail::Int32)
    {
      attrMat->createAndAddAttributeArray<DataArray<int32_t>, AbstractFilter, int32_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(int32_t);
    }
    else if (m_ScalarType == Detail::UInt32)
    {
      attrMat->createAndAddAttributeArray<DataArray<uint32_t>, AbstractFilter, uint32_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(uint32_t);
    }
    else if (m_ScalarType == Detail::Int64)
    {
      attrMat->createAndAddAttributeArray<DataArray<int64_t>, AbstractFilter, int64_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(int64_t);
    }
    else if (m_ScalarType == Detail::UInt64)
    {
      attrMat->createAndAddAttributeArray<DataArray<uint64_t>, AbstractFilter, uint64_t>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(uint64_t);
    }
    else if (m_ScalarType == Detail::Float)
    {
      attrMat->createAndAddAttributeArray<DataArray<float>, AbstractFilter, float>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(float);
    }
    else if (m_ScalarType == Detail::Double)
    {
      attrMat->createAndAddAttributeArray<DataArray<double>, AbstractFilter, double>(this, m_OutputArrayName, 0, dims);
      allocatedBytes *= sizeof(double);
    }

    // Sanity Check Allocated Bytes versus size of file
    uint64_t fileSize = fi.size();
    int check = SanityCheckFileSizeVersusAllocatedSize(allocatedBytes, fileSize, m_SkipHeaderBytes);
    if (check == -1)
    {

      QString ss = QObject::tr("The file size is %1 but the number of bytes needed to fill the array is %2. This condition would cause an error reading the input file."
                               " Please adjust the input parameters to match the size of the file or select a different data file.").arg(fileSize).arg(allocatedBytes);
      setErrorCondition(RBR_FILE_TOO_SMALL);
      notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
    }
    else if (check == 1)
    {

      QString ss = QObject::tr("The file size is %1 but the number of bytes needed to fill the array is %2 which is less than the size of the file."
                               " DREAM3D will read only the first part of the file into the array.").arg(fileSize).arg(allocatedBytes);
      notifyWarningMessage(getHumanLabel(), ss, RBR_FILE_TOO_BIG);
    }
  }
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void RawBinaryReader::preflight()
{
  setInPreflight(true);
  emit preflightAboutToExecute();
  emit updateFilterParameters(this);
  dataCheck(true);
  emit preflightExecuted();
  setInPreflight(false);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void RawBinaryReader::execute()
{
  int err = 0;
  QString ss;
  setErrorCondition(err);

  dataCheck(false);
  if(getErrorCondition() < 0)
  {
    return;
  }


  DataContainer::Pointer m;
  if(getAddToExistingDataContainer())
  {
    //existing data container, get name from correct place (depends on if we will also use existing attribute matrix)
    if(getAddToExistingAttributeMatrix())
    {
      //existing data continaer and attirbute matrix
      m = getDataContainerArray()->getDataContainer(getAttributeMatrixPath().getDataContainerName());
    }
    else
    {
      //existing data continaer but new attirbute matrix
      m = getDataContainerArray()->getDataContainer(getExistingDataContainerName());
    }
    if(getErrorCondition() < 0)
    {
      return;
    }
  }
  else
  {
    //new data container
    m = getDataContainerArray()->getDataContainer(getNewDataContainerName());
    if(getErrorCondition() < 0)
    {
      return;
    }
  }

  //set dimensions of data container if creating new
  if(!getAddToExistingDataContainer())
  {
    DataContainer::Pointer m = getDataContainerArray()->getDataContainer(getNewDataContainerName());
    ImageGeom::Pointer image = ImageGeom::CreateGeometry("BinaryImage");
    image->setOrigin(m_Origin.x, m_Origin.y, m_Origin.z);
    image->setResolution(m_Resolution.x, m_Resolution.y, m_Resolution.z);
    image->setDimensions(m_Dimensions.x, m_Dimensions.y, m_Dimensions.z);
    m->setGeometry(image);
  }

  setErrorCondition(0);

  // Get the total size of the array from the options
  size_t voxels;
  QVector<size_t> tDims;
  QString attributeMatrixName;
  switch(getAttributeMatrixType())
  {
    //cell
    case 0:
      {
        voxels = m_Dimensions.x * m_Dimensions.y * m_Dimensions.z;
        tDims = QVector<size_t>(3, 0);
        tDims[0] = m_Dimensions.x;
        tDims[1] = m_Dimensions.y;
        tDims[2] = m_Dimensions.z;
        attributeMatrixName = getCellAttributeMatrixName();
      }
      break;

    //cell feature
    case 1:
      {
        voxels = getNumFeatures();
        tDims = QVector<size_t>(1, voxels);
        attributeMatrixName = getCellFeatureAttributeMatrixName();
      }
      break;


    //cell ensemble
    case 2:
      {
        voxels = getNumPhases();
        tDims = QVector<size_t>(1, voxels);
        attributeMatrixName = getCellEnsembleAttributeMatrixName();
      }
      break;

    default:
      //future location for other types
      QString ss = QObject::tr("Unsupported AttributeMatrix type selected.");
      setErrorCondition(-1);
      notifyErrorMessage(getHumanLabel(), ss, getErrorCondition());
      return;
  }

  //resize attribute matrix if creating new
  if(!getAddToExistingAttributeMatrix())
  {
    m->getAttributeMatrix(attributeMatrixName)->resizeAttributeArrays(tDims);
  }

  array = IDataArray::NullPointer();
  if (m_ScalarType == Detail::Int8)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    Int8ArrayType::Pointer p = Int8ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<int8_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::UInt8)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    UInt8ArrayType::Pointer p = UInt8ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<uint8_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::Int16)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    Int16ArrayType::Pointer p = Int16ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<int16_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::UInt16)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    UInt16ArrayType::Pointer p = UInt16ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<uint16_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::Int32)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    Int32ArrayType::Pointer p = Int32ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<int32_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::UInt32)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    UInt32ArrayType::Pointer p = UInt32ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<uint32_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::Int64)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    Int64ArrayType::Pointer p = Int64ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<int64_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::UInt64)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    UInt64ArrayType::Pointer p = UInt64ArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<uint64_t>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::Float)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    FloatArrayType::Pointer p = FloatArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    p->initializeWithValue(666.6666f);
    err = readBinaryFile<float>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }
  else if (m_ScalarType == Detail::Double)
  {
    QVector<size_t> dims(1, m_NumberOfComponents);
    DoubleArrayType::Pointer p = DoubleArrayType::CreateArray(voxels, dims, m_OutputArrayName);
    err = readBinaryFile<double>(p, m_InputFile, m_SkipHeaderBytes);
    if (err >= 0 )
    {
      SWAP_ARRAY(p)
      array = p;
    }
  }

  if (NULL != array.get())
  {
    m->getAttributeMatrix(attributeMatrixName)->addAttributeArray(array->getName(), array);
  }
  else if(err == RBR_FILE_NOT_OPEN )
  {
    setErrorCondition(RBR_FILE_NOT_OPEN);
    notifyErrorMessage(getHumanLabel(), "RawBinaryReader was unable to open the specified file.", getErrorCondition());
  }
  else if (err == RBR_FILE_TOO_SMALL)
  {
    setErrorCondition(RBR_FILE_TOO_SMALL);
    notifyErrorMessage(getHumanLabel(), "The file size is smaller than the allocated size.", getErrorCondition());
  }
  else if (err == RBR_FILE_TOO_BIG)
  {
    notifyWarningMessage(getHumanLabel(), "The file size is larger than the allocated size.", RBR_FILE_TOO_BIG);
  }
  else if(err == RBR_READ_EOF)
  {
    setErrorCondition(RBR_READ_EOF);
    notifyErrorMessage(getHumanLabel(), "RawBinaryReader read past the end of the specified file.", getErrorCondition());
  }

  /* Let the GUI know we are done with this filter */
  notifyStatusMessage(getHumanLabel(), "Complete");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer RawBinaryReader::newFilterInstance(bool copyFilterParameters)
{
  RawBinaryReader::Pointer filter = RawBinaryReader::New();
  if(true == copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString RawBinaryReader::getCompiledLibraryName()
{
  return Core::CoreBaseName;
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString RawBinaryReader::getGroupName()
{
  return DREAM3D::FilterGroups::IOFilters;
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString RawBinaryReader::getSubGroupName()
{
  return DREAM3D::FilterSubGroups::InputFilters;
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
const QString RawBinaryReader::getHumanLabel()
{
  return "Raw Binary Reader";
}

