/* ShapeWorks License */

#include <ShapeWorksView2.h>
#include <ui_ShapeWorksView2.h>

// standard includes
#include <iostream>
#include <sstream>

// vnl
#include "vnl/vnl_vector.h"
#include "vnl/algo/vnl_symmetric_eigensystem.h"
#include "vnl/vnl_matrix.h"

// vtk
#include <vtkActor.h>
#include <vtkArrowSource.h>
#include <vtkColorTransferFunction.h>
#include <vtkContourFilter.h>
#include <vtkDecimatePro.h>
#include <vtkFloatArray.h>
#include <vtkGlyph3D.h>
#include <vtkImageConstantPad.h>
#include <vtkImageData.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageGradient.h>
#include <vtkImageWriter.h>
#include <vtkLookupTable.h>
#include <vtkPLYReader.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkReverseSense.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkSphereSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkUnsignedLongArray.h>

// local libraries
#include "tinyxml.h"
#include "CustomSurfaceReconstructionFilter.h"
#include <Preferences.h>

// local files
#ifdef SW_USE_POWERCRUST
#include "vtkPowerCrustSurfaceReconstruction.h"
#endif

ShapeWorksView2::ShapeWorksView2( int argc, char** argv )
{
  this->ui = new Ui_ShapeWorksView2;
  this->ui->setupUi( this );

  QSize size = Preferences::Instance().getSettings().value( "mainwindow/size", QSize( 1280, 720 ) ).toSize();
  this->resize( size );

  if ( !this->readParameterFile( argv[1] ) )
  {
    exit( -1 );
  }

  this->updateShapeMode();
}

ShapeWorksView2::~ShapeWorksView2()
{}

/********************************************************************/
/* Qt event handling                                                */
/********************************************************************/

void ShapeWorksView2::closeEvent( QCloseEvent* event )
{
  Preferences::Instance().getSettings().setValue( "mainwindow/size", this->size() );
}

void ShapeWorksView2::on_actionQuit_triggered()
{
  this->close();
}

void ShapeWorksView2::on_actionPreferences_triggered()
{
  Preferences::Instance().showWindow();
}

void ShapeWorksView2::on_meanButton_clicked()
{
  this->updateShapeMode();
}

void ShapeWorksView2::on_sampleButton_clicked()
{
  this->updateShapeMode();
}

void ShapeWorksView2::on_pcaButton_clicked()
{
  this->updateShapeMode();
}

void ShapeWorksView2::on_meanOverallButton_clicked()
{
  this->updateShapeMode();
}

void ShapeWorksView2::on_meanGroup1Button_clicked()
{
  this->updateShapeMode();
}

void ShapeWorksView2::on_meanGroup2Button_clicked()
{
  this->updateShapeMode();
}

void ShapeWorksView2::on_sampleSpinBox_valueChanged()
{
  // this will make the UI appear more responsive
  QCoreApplication::processEvents();

  this->updateShapeMode();
}

void ShapeWorksView2::on_medianButton_clicked()
{
  int sampleNumber = this->stats.ComputeMedianShape( -32 );       // -32 means use both groups
  this->ui->sampleSpinBox->setValue( sampleNumber );
}

void ShapeWorksView2::on_medianGroup1Button_clicked()
{
  int sampleNumber = this->stats.ComputeMedianShape( 1 );
  this->ui->sampleSpinBox->setValue( sampleNumber );
}

void ShapeWorksView2::on_medianGroup2Button_clicked()
{
  int sampleNumber = this->stats.ComputeMedianShape( 2 );
  this->ui->sampleSpinBox->setValue( sampleNumber );
}

void ShapeWorksView2::on_pcaSlider_valueChanged()
{
  // this will make the slider handle redraw making the UI appear more responsive
  QCoreApplication::processEvents();

  this->computeModeShape();
  this->redraw();
}

void ShapeWorksView2::on_pcaModeSpinBox_valueChanged()
{
  this->computeModeShape();
  this->redraw();
}

void ShapeWorksView2::on_pcaGroupSlider_valueChanged()
{
  // this will make the UI appear more responsive
  QCoreApplication::processEvents();

  this->computeModeShape();
  this->redraw();
}

void ShapeWorksView2::on_showGlyphs_stateChanged()
{
  this->updateActors();
}

void ShapeWorksView2::on_showSurface_stateChanged()
{
  this->updateActors();
}

void ShapeWorksView2::on_usePowerCrustCheckBox_stateChanged()
{
  this->updateSurfaceSettings();
  this->redraw();
}

void ShapeWorksView2::on_neighborhoodSpinBox_valueChanged()
{
  this->updateSurfaceSettings();
  this->redraw();
}

void ShapeWorksView2::on_spacingSpinBox_valueChanged()
{
  this->updateSurfaceSettings();
  this->redraw();
}

/********************************************************************/
/* private methods                                                  */
/********************************************************************/

void ShapeWorksView2::initializeRenderer()
{
  // Set up renderer and interactor.
  this->renderer = vtkSmartPointer<vtkRenderer>::New();

  this->ui->view->GetRenderWindow()->AddRenderer( this->renderer );

  vnl_vector<double> col( 3 );
  //col[0] = 255;
  //col[1] = 239;
  //col[2] = 213;
  col[0] = 255;
  col[1] = 239;
  col[2] = 213;
  col.normalize();
  this->renderer->SetBackground( col[0], col[1], col[2] );
}

void ShapeWorksView2::initializeGlyphs()
{
  this->lut = vtkSmartPointer<vtkLookupTable>::New();
  this->scalars = vtkSmartPointer<vtkUnsignedLongArray>::New();
  this->sphereSource = vtkSmartPointer<vtkSphereSource>::New();

  this->glyphPoints = vtkSmartPointer<vtkPoints>::New();
  this->glyphPoints->SetDataTypeToDouble();
  this->glyphPointSet = vtkSmartPointer<vtkPolyData>::New();
  this->glyphPointSet->SetPoints( this->glyphPoints );
  this->glyphPointSet->GetPointData()->SetScalars( this->scalars );

  this->glyphs = vtkSmartPointer<vtkGlyph3D>::New();
  this->glyphs->SetInput( this->glyphPointSet );
  this->glyphs->ScalingOn();
  this->glyphs->ClampingOff();
  this->glyphs->SetScaleModeToDataScalingOff();
  this->glyphs->SetSourceConnection( this->sphereSource->GetOutputPort() );
  this->glyphs->SetScaleModeToDataScalingOff();

  this->glyphMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->glyphMapper->SetInputConnection( this->glyphs->GetOutputPort() );
  this->glyphMapper->SetLookupTable( this->lut );

  this->glyphActor = vtkSmartPointer<vtkActor>::New();
  this->glyphActor->GetProperty()->SetSpecularColor( 1.0, 1.0, 1.0 );
  this->glyphActor->GetProperty()->SetDiffuse( 0.8 );
  this->glyphActor->GetProperty()->SetSpecular( 0.3 );
  this->glyphActor->GetProperty()->SetSpecularPower( 10.0 );
  this->glyphActor->SetMapper( this->glyphMapper );

  //arrowFlipFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  //arrowGlyphs = vtkGlyph3D::New();
  //arrowGlyphMapper = vtkPolyDataMapper::New();
  //arrowGlyphActor = vtkActor::New();

  //this->pValueTFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
  //this->pValueTFunc->SetColorSpaceToHSV();
  //this->pValueTFunc->AddHSVPoint( 0, 0, 1, 1 );
  //this->pValueTFunc->AddHSVPoint( 0.05, 1, .40, 1 );
  //this->pValueTFunc->AddHSVPoint( 0.05 + 0.00001, 1, 0.0, 1 );
  //this->pValueTFunc->AddHSVPoint( 1, 1, 0.0, 1 );

  //this->differenceLUT = vtkSmartPointer<vtkColorTransferFunction>::New();
  //this->differenceLUT->SetColorSpaceToHSV();

  // Arrow glyphs
  //this->arrowSource = vtkSmartPointer<vtkArrowSource>::New();
  //this->arrowSource->SetTipResolution( 6 );
  //this->arrowSource->SetShaftResolution( 6 );

  //vtkTransform* t1 = vtkTransform::New();
  //vtkTransform* t2 = vtkTransform::New();
  //vtkTransform* t3 = vtkTransform::New();
  //vtkTransform* t4 = vtkTransform::New();
  //t1->Translate( -0.5, 0.0, 0.0 );
  //t2->RotateY( 180 );
  //t3->Translate( 0.5, 0.0, 0.0 );
  //t4->RotateY( 180 );
  //t3->Concatenate( t4 );
  //t2->Concatenate( t3 );
  //t1->Concatenate( t2 );
  //this->transform180 = vtkSmartPointer<vtkTransform>::New();
  //this->transform180->Concatenate( t1 );
  //t1->Delete();
  //t2->Delete();
  //t3->Delete();
  //t4->Delete();

  /*
     m_arrowFlipFilter->SetTransform(m_transform180);
     m_arrowFlipFilter->SetInputConnection(m_arrowSource->GetOutputPort());

     m_arrowGlyphs->SetSourceConnection(m_arrowFlipFilter->GetOutputPort());
     m_arrowGlyphs->SetInput(m_glyphPointset);
     m_arrowGlyphs->ScalingOn();
     m_arrowGlyphs->ClampingOff();

     m_arrowGlyphs->SetVectorModeToUseVector();
     m_arrowGlyphs->SetScaleModeToScaleByVector();

     m_arrowGlyphMapper->SetInputConnection(m_arrowGlyphs->GetOutputPort());

     m_arrowGlyphActor->GetProperty()->SetSpecularColor(1.0, 1.0, 1.0);
     m_arrowGlyphActor->GetProperty()->SetDiffuse(0.8);
     m_arrowGlyphActor->GetProperty()->SetSpecular(0.3);
     m_arrowGlyphActor->GetProperty()->SetSpecularPower(10.0);
     m_arrowGlyphActor->SetMapper(m_arrowGlyphMapper);
   */
}

void ShapeWorksView2::initializeSurface()
{
  this->surface = vtkSmartPointer<CustomSurfaceReconstructionFilter>::New();
  this->surface->SetInput( this->glyphPointSet );

#ifdef SW_USE_POWERCRUST
  this->powercrust = vtkSmartPointer<vtkPowerCrustSurfaceReconstruction>::New();
  this->powercrust->SetInput( this->glyphPointSet );
#endif

  this->surfaceContourFilter = vtkSmartPointer<vtkContourFilter>::New();
  this->surfaceContourFilter->SetInputConnection( this->surface->GetOutputPort() );
  this->surfaceContourFilter->SetValue( 0, 0.0 );
  this->surfaceContourFilter->ComputeNormalsOn();

  this->surfaceReverseSense = vtkSmartPointer<vtkReverseSense>::New();
  this->surfaceReverseSense->SetInputConnection( this->surfaceContourFilter->GetOutputPort() );
  this->surfaceReverseSense->ReverseCellsOn();
  this->surfaceReverseSense->ReverseNormalsOn();

  this->surfaceSmoothFilter = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
  this->surfaceSmoothFilter->SetInputConnection( this->surfaceReverseSense->GetOutputPort() );
  this->surfaceSmoothFilter->SetNumberOfIterations( 0 );

  this->polydataNormals = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->polydataNormals->SplittingOff();
  this->polydataNormals->SetInputConnection( this->surfaceSmoothFilter->GetOutputPort() );

  this->surfaceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->surfaceMapper->SetInputConnection( this->polydataNormals->GetOutputPort() );
  this->surfaceMapper->ScalarVisibilityOff();

  this->surfaceActor = vtkSmartPointer<vtkActor>::New();
  this->surfaceActor->SetMapper( this->surfaceMapper );
  this->surfaceActor->GetProperty()->SetSpecular( .4 );
  this->surfaceActor->GetProperty()->SetSpecularPower( 50 );

  this->updateSurfaceSettings();
}

void ShapeWorksView2::updateShapeMode()
{
  // update UI
  this->ui->meanWidget->setVisible( this->groupsAvailable && this->ui->meanButton->isChecked() );
  this->ui->sampleWidget->setVisible( this->ui->sampleButton->isChecked() );
  this->ui->pcaWidget->setVisible( this->ui->pcaButton->isChecked() );
  this->ui->medianGroup1Button->setVisible( this->groupsAvailable );
  this->ui->medianGroup2Button->setVisible( this->groupsAvailable );
  this->ui->pcaGroup1Label->setVisible( this->groupsAvailable );
  this->ui->pcaGroup2Label->setVisible( this->groupsAvailable );
  this->ui->pcaGroupSlider->setVisible( this->groupsAvailable );

  // this will make the UI appear more responsive
  QCoreApplication::processEvents();

#ifndef SW_USE_POWERCRUST
  this->ui->powerCrustLabel->hide();
  this->ui->usePowerCrustCheckBox->hide();
#endif

  if ( this->ui->meanButton->isChecked() )
  {
    if ( this->ui->meanGroup1Button->isChecked() )
    {
      this->displayShape( this->stats.Group1Mean() );
    }
    else if ( this->ui->meanGroup2Button->isChecked() )
    {
      this->displayShape( this->stats.Group2Mean() );
    }
    else
    {
      this->displayShape( this->stats.Mean() );
    }
  }
  else if ( this->ui->sampleButton->isChecked() )
  {
    int sampleNumber = this->ui->sampleSpinBox->value();
    this->displayShape( this->stats.ShapeMatrix().get_column( sampleNumber ) );
  }
  else if ( this->ui->pcaButton->isChecked() )
  {
    this->computeModeShape();
  }

  this->redraw();
}

void ShapeWorksView2::updateSurfaceSettings()
{
  this->surface->SetNeighborhoodSize( this->ui->neighborhoodSpinBox->value() );
  this->surface->SetSampleSpacing( this->ui->spacingSpinBox->value() );
  this->surface->Modified();

  // clear the cache since the surface reconstruction parameters have changed
  this->modelCache.clear();

  bool powercrust = this->ui->usePowerCrustCheckBox->isChecked();

  // update UI
  this->ui->neighborhoodSpinBox->setEnabled( !powercrust );
  this->ui->spacingSpinBox->setEnabled( !powercrust );

  if ( powercrust )
  {
#ifdef SW_USE_POWERCRUST
    this->surfaceReverseSense->SetInputConnection( this->powercrust->GetOutputPort() );
#endif
  }
  else
  {
    this->surfaceReverseSense->SetInputConnection( this->surfaceContourFilter->GetOutputPort() );
  }

  this->displayShape( this->currentShape );
}

void ShapeWorksView2::updateActors()
{
  this->renderer->RemoveActor( this->glyphActor );
  this->renderer->RemoveActor( this->surfaceActor );

  if ( this->ui->showGlyphs->isChecked() )
  {
    this->renderer->AddActor( this->glyphActor );
  }

  if ( this->ui->showSurface->isChecked() )
  {
    this->renderer->AddActor( this->surfaceActor );
  }

  this->displayShape( this->currentShape );

  this->redraw();
}

void ShapeWorksView2::redraw()
{
  this->renderer->GetRenderWindow()->Render();
}

bool ShapeWorksView2::readParameterFile( char* filename )
{
  // Read parameter file
  TiXmlDocument doc( filename );
  bool loadOkay = doc.LoadFile();
  if ( !loadOkay )
  {
    std::cerr << "Error: Invalid parameter file" << std::endl;
    return false;
  }

  TiXmlHandle docHandle( &doc );
  TiXmlElement*      elem;
  std::istringstream inputsBuffer;

  elem = docHandle.FirstChild( "group_ids" ).Element();
  if ( elem )
  {
    this->groupsAvailable = true;
  }
  else
  {
    this->groupsAvailable = false;
  }

  // Run statistics
  this->stats.ReadPointFiles( filename );
  this->stats.ComputeModes();
  this->stats.PrincipalComponentProjections();

  unsigned int numPoints = this->stats.Mean().size();

  this->numSamples = this->stats.ShapeMatrix().cols();

  this->initializeRenderer();
  this->initializeGlyphs();
  this->initializeSurface();

  // Create numPoints glyphs
  for ( unsigned int i = 0; i < numPoints / 3; i++ )
  {
    this->glyphs->SetRange( 0.0, (double) i + 1 );
    this->glyphMapper->SetScalarRange( 0.0, (double) i + 1.0 );
    this->lut->SetNumberOfTableValues( i + 1 );
    this->lut->SetTableRange( 0.0, (double)i + 1.0 );

    ( (vtkUnsignedLongArray*)( this->glyphPointSet->GetPointData()->GetScalars() ) )->InsertValue( i, i );

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    this->glyphPoints->InsertPoint( i, x, y, z );
  }

  this->glyphPoints->Modified();

  this->displayShape( this->stats.Mean() );

  this->ui->sampleSpinBox->setMinimum( 0 );
  this->ui->sampleSpinBox->setMaximum( this->numSamples - 1 );

  this->updateActors();

  return true;
}

void ShapeWorksView2::displayShape( const vnl_vector<double> &shape )
{
  if ( shape.size() == 0 )
  {
    return;
  }

  // make a copy of the shape
  this->currentShape = shape;

  unsigned int k = 0;
  for ( unsigned int i = 0; i < this->stats.ShapeMatrix().rows() / 3; i++ )
  {
    double x = shape[k++];
    double y = shape[k++];
    double z = shape[k++];
    this->glyphPoints->SetPoint( i, x, y, z );
  }
  this->glyphPoints->Modified();

  if ( surface && surfaceActor && this->ui->showSurface->isChecked() )
  {

    vtkSmartPointer<vtkPolyData> polyData;

    if ( this->modelCache.getModel( shape ) )
    {
      polyData = this->modelCache.getModel( shape );
    }
    else
    {
      if ( !this->ui->usePowerCrustCheckBox->isChecked() )
      {
        this->surface->Modified();
        this->surface->Update();
        this->surfaceContourFilter->Update();
      }

      this->polydataNormals->Update();

      // make a copy of the vtkPolyData output and place it in the cache
      polyData = vtkSmartPointer<vtkPolyData>::New();
      polyData->DeepCopy( this->polydataNormals->GetOutput() );
      this->modelCache.insertModel( shape, polyData );
    }

    // retrieve the model from the cache and set it for display
    this->surfaceMapper->SetInput( polyData );
  }
}

void ShapeWorksView2::computeModeShape()
{
  double pcaSliderValue = this->ui->pcaSlider->value() / 10.0;

  unsigned int m = this->stats.Eigenvectors().columns() - ( this->ui->pcaModeSpinBox->value() + 1 );

  vnl_vector<double> e = this->stats.Eigenvectors().get_column( m );

  double lambda = sqrt( this->stats.Eigenvalues()[m] );

  this->ui->pcaValueLabel->setText( QString::number( pcaSliderValue ) );
  this->ui->pcaEigenValueLabel->setText( QString::number( this->stats.Eigenvalues()[m] ) );
  this->ui->pcaLambdaLabel->setText( QString::number( pcaSliderValue * lambda ) );

  if ( this->stats.GroupID().size() > 0 )
  {
    double groupSliderValue = this->ui->pcaGroupSlider->value();
    double ratio = groupSliderValue / static_cast<double>( this->ui->pcaGroupSlider->maximum() );
    this->displayShape( this->stats.Group1Mean() + ( this->stats.GroupDifference() * ratio ) + ( e * ( pcaSliderValue * lambda ) ) );
  }
  else
  {
    this->displayShape( this->stats.Mean() + ( e * ( pcaSliderValue * lambda ) ) );
  }
}