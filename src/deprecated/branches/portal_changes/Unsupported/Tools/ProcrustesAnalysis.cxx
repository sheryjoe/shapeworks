/*=========================================================================
  Program:   ShapeWorks: Particle-based Shape Correspondence & Visualization
  Module:    $RCSfile: ProcrustesAnalysis.cxx,v $
  Date:      $Date: 2009/05/06 21:49:16 $
  Version:   $Revision: 1.1.1.1 $
  Author:    $Author: cates $

  Copyright (c) 2009 Scientific Computing and Imaging Institute.
  See ShapeWorksLicense.txt for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.
=========================================================================*/
#include <iostream>
#include <vector>
#include <string>
#include "itkParticlePositionReader.h"
#include "itkParticlePositionWriter.h"
#include "Procrustes3D.h"
#include "itkParticleSystem.h"
#include "object_writer.h"
#include "object_reader.h"
#include "tinyxml.h"
#include <sstream>
#include "RotationGeometry.hxx"
#include <fstream>

int main(int argc, char *argv[])
{
  typedef itk::ParticleSystem<3> ParticleSystemType;
  typedef RotationGeometry<double>::PointType QuaternionType;
  typedef RotationGeometry<double>::TangentType VectorType;
  typedef itk::ParticleSystem<3>::TransformType TransformType;
  typedef vnl_vector_fixed<double, 3> vectype;

  if (argc != 2)
    {
    std::cerr << "Use: " << argv[0] << "paramfile" << std::endl;
    return 1;
    }

  TiXmlDocument doc(argv[1]);
  bool loadOkay = doc.LoadFile();
  TiXmlHandle docHandle( &doc );
  TiXmlElement *elem;
  std::istringstream inputsBuffer;

  std::string outputfile;

  std::vector< std::string > inputfiles;
  std::vector< std::vector< QuaternionType > > rotations;
  std::vector< std::vector< vectype > > translations;
  std::vector< std::vector< double > > scales;
  std::vector< std::vector< TransformType > > transformlist;
  std::vector< int > groups;
  
  std::string tmpa;
  std::string transforms_output, localtrans_filename;
  unsigned int domains_per_shape;

  //PARAMSET(pf, transforms_output, "transforms_output", 0, ok, "");
  transforms_output = "";
  elem = docHandle.FirstChild( "transforms_output" ).Element();
  if (elem)
    transforms_output = elem->GetText();
  else
  {
    std::cerr << "Missing parameters" << std::endl;
    return 1;
  }

  //PARAMSET(pf, domains_per_shape, "domains_per_shape", 0, ok, 1);
  domains_per_shape = 1;
  elem = docHandle.FirstChild( "domains_per_shape" ).Element();
  if (elem)
    domains_per_shape = atoi(elem->GetText());
  else
  {
    std::cerr << "Missing parameters" << std::endl;
    return 1;
  }

  elem = docHandle.FirstChild( "inputs" ).Element();
  if (!elem)
  {
    std::cerr << "No input files have been specified" << std::endl;
    throw 1;
  }
  else
  {
    inputsBuffer.str(elem->GetText());
    while (inputsBuffer >> tmpa)
    {
      inputfiles.push_back(tmpa);
    }
    inputsBuffer.clear();
    inputsBuffer.str("");
  }

  // Read the translations.
  //PARAMSET(pf, localtrans_filename, "local_translations", 0, ok, "");
  object_reader< itk::ParticleSystem<3>::TransformType > transreader;
  localtrans_filename = "";
  bool readtrans = false;
  elem = docHandle.FirstChild( "local_translations" ).Element();
  if (elem)
  {
    readtrans = true;
    localtrans_filename = elem->GetText();
    transreader.SetFileName(localtrans_filename.c_str());
    transreader.Update();
  }

  const int numShapes = inputfiles.size() / domains_per_shape;

  // Read the group ids
  int g;
  elem = docHandle.FirstChild( "group_ids" ).Element();
  if (elem)
  {
    inputsBuffer.str(elem->GetText());
    for (unsigned int i = 0; i < numShapes; i++)
    {
      if (inputsBuffer >> g) groups.push_back(g);
    }
    inputsBuffer.clear();
    inputsBuffer.str("");
  }

  // Run procrustes on each structure (domain) individually
  for (unsigned int s = 0; s < domains_per_shape ; s++)
    {
    Procrustes3D::ShapeListType shapelist;
    Procrustes3D::ShapeType     shapevector;
    Procrustes3D::PointType     point;

    std::vector<double> tmpscales;
    std::vector<QuaternionType> tmprotations;
    std::vector<vectype> tmptranslations;
    std::vector<TransformType> tmptransforms;
    std::vector<TransformType> tmptranslationslist;

    // Read input shapes from file list
    for(int d = s; d < inputfiles.size(); d+=domains_per_shape)
      {
      // grab a list of the tranlations for this structure
      if (readtrans == true)
        tmptranslationslist.push_back(transreader.GetOutput()[d]);

      shapevector.clear();
      
      itk::ParticlePositionReader<3>::Pointer reader = itk::ParticlePositionReader<3>::New();
      reader->SetFileName(inputfiles[d].c_str());
      reader->Update();
      
      for(int j = 0; j < reader->GetOutput().size(); j++)
        {
        point(0) = (reader->GetOutput()[j])[0];
        point(1) = (reader->GetOutput()[j])[1];
        point(2) = (reader->GetOutput()[j])[2];
        
        shapevector.push_back(point);
        }
      shapelist.push_back(shapevector);

      }

    // Run alignment
    Procrustes3D::SimilarityTransformListType transforms;
    Procrustes3D procrustes;
    procrustes.AlignShapes(transforms, shapelist);
    
    // Transform shapes
    //    Procrustes3D::TransformShapes(shapelist, transforms);
    
    // Record the rotation, scale and translation information for each
    for (int i = 0; i < numShapes; i++)
      {
      // Record the scale information
      tmpscales.push_back(transforms[i].scale);
      
      // Record the full transform matrix
      ParticleSystemType::TransformType R;
      R(0,0) =  transforms[i].rotation(0,0) * transforms[i].scale;
      R(1,0) =  transforms[i].rotation(1,0) * transforms[i].scale;
      R(2,0) =  transforms[i].rotation(2,0) * transforms[i].scale;
      R(3,0) =  0.0;
      
      R(0,1) =  transforms[i].rotation(0,1) * transforms[i].scale;
      R(1,1) =  transforms[i].rotation(1,1) * transforms[i].scale;
      R(2,1) =  transforms[i].rotation(2,1) * transforms[i].scale;
      R(3,1) =  0.0;
      
      R(0,2) =  transforms[i].rotation(0,2) * transforms[i].scale;
      R(1,2) =  transforms[i].rotation(1,2) * transforms[i].scale;
      R(2,2) =  transforms[i].rotation(2,2) * transforms[i].scale;
      R(3,2) =  0.0;
      
      R(0,3) =  transforms[i].translation(0);
      R(1,3) =  transforms[i].translation(1);
      R(2,3) =  transforms[i].translation(2);
      R(3,3) =  1.0;
      tmptransforms.push_back(R);
      
      // Record the quaternion (rotation) component
      // Construct a quaternion from the rotation
      vnl_matrix_fixed<double, 3, 3> r;
      r(0,0) = transforms[i].rotation(0, 0);
      r(0,1) = transforms[i].rotation(0, 1);
      r(0,2) = transforms[i].rotation(0, 2);
      r(1,0) = transforms[i].rotation(1, 0);
      r(1,1) = transforms[i].rotation(1, 1);
      r(1,2) = transforms[i].rotation(1, 2);
      r(2,0) = transforms[i].rotation(2, 0);
      r(2,1) = transforms[i].rotation(2, 1);
      r(2,2) = transforms[i].rotation(2, 2);
      QuaternionType tmpQ(r);
      tmpQ.normalize();
      tmprotations.push_back(tmpQ);
      
      // Record the translation component.  Translation also includes
      // the information from the localtranslations file.
      vectype trans;
      if (readtrans == true)
        {
        trans[0] = transforms[i].translation(0) + tmptranslationslist[i](0,3);
        trans[1] = transforms[i].translation(1) + tmptranslationslist[i](1,3);
        trans[2] = transforms[i].translation(2) + tmptranslationslist[i](2,3);
        }
      else
        {
        trans[0] = transforms[i].translation(0);
        trans[1] = transforms[i].translation(1);
        trans[2] = transforms[i].translation(2);
        }
      tmptranslations.push_back(trans);
      } // for each structure

    rotations.push_back(tmprotations);
    translations.push_back(tmptranslations);
    scales.push_back(tmpscales);
    transformlist.push_back(tmptransforms);
    }

  // Now we have all information stored in a matrix with rows representing the
  // different structures, and columns representing the individual samples
  // ROW: Structure
  // COL: Sample
  
  // Now write the log file.

   // First write all scales in order of the input files
   std::cout << "SCALES" << std::endl;
   for (unsigned int i = 0; i < numShapes; i++) // loop over samples
     for (unsigned int j = 0; j < domains_per_shape; j++) // loop over structure
       {
       std::cout << scales[j][i] << std::endl;
       }

   // Now list the scales by structure
   std::cout << std::endl;
   std::cout << "SCALES BY STRUCTURE" << std::endl;
   for (unsigned int i = 0; i < domains_per_shape; i++)
     {
     std::cout << "STRUCTURE " << i << std::endl;
     for (unsigned int j = 0; j < numShapes; j++)
       {
       std::cout << scales[i][j] << std::endl;
       }
     std::cout << std::endl;
     }

   // Now list the quaternions by structure
   std::cout << std::endl;
   std::cout << "QUATERNIONS BY STRUCTURE" << std::endl;
   for (unsigned int i = 0; i < domains_per_shape; i++)
     {
     std::cout << "STRUCTURE " << i << std::endl;
     for (unsigned int j = 0; j < numShapes; j++)
       {
       std::cout << rotations[i][j] << std::endl;
       }
     std::cout << std::endl;
     }

  // Now list the translations by structure
   std::cout << std::endl;
   std::cout << "TRANSLATIONS BY STRUCTURE" << std::endl;
   for (unsigned int i = 0; i < domains_per_shape; i++)
     {
     std::cout << "STRUCTURE " << i << std::endl;
     for (unsigned int jj = 0; jj < 3; jj++)
       {
       std::cout << "axis " << jj << std::endl;
       for (unsigned int j = 0; j < numShapes; j++)
         {
         std::cout << translations[i][j](jj) << std::endl;
           //       std::cout << translations[i][j] << std::endl;
         }
       }
     std::cout << std::endl;
     }
 
 // Write csv file
  std::ofstream outfile;
  outfile.open("procrustes.txt");
  outfile << "Group";
  for (unsigned int i = 0; i < domains_per_shape; i++)
    {
    outfile << ",S" << i;
    }
  for (unsigned int i = 0; i < domains_per_shape; i++)
    {
    outfile << ",Tx" << i;
    outfile << ",Ty" << i;
    outfile << ",Tz" << i;
    }
  outfile << std::endl;
  for (unsigned int j = 0; j < numShapes; j++)
    {
    outfile << groups[j];
    for (unsigned int i = 0; i < domains_per_shape; i++)
      {      
      outfile << "," << scales[i][j];
      }
    for (unsigned int i = 0; i < domains_per_shape; i++)
      {      
      outfile << "," << translations[i][j][0];
      outfile << "," << translations[i][j][1];
      outfile << "," << translations[i][j][2];
      }
    outfile << std::endl;
    }
  
  
  outfile.close();
  
  return 0;
}

