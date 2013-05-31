
#ifndef __DOLFIN_BOOST_TYPES_H
#define __DOLFIN_BOOST_TYPES_H

#include <dolfin.h>
#include "petscsnes.h"

namespace buckettools {

  //*****************************************************************|************************************************************//
  // A collection of typedefs for boost pointers to dolfin, petsc and bucket structures
  //*****************************************************************|************************************************************//

  //*****************************************************************|************************************************************//
  // boost shared pointers to basic objects
  //*****************************************************************|************************************************************//

  typedef boost::shared_ptr< int >    int_ptr;
  typedef boost::shared_ptr< double > double_ptr;
  typedef boost::shared_ptr< bool >   bool_ptr;

  //*****************************************************************|************************************************************//
  // boost shared pointers to bucket objects
  //*****************************************************************|************************************************************//

  class Bucket;                                                      // predeclaration
  typedef boost::shared_ptr< Bucket >           Bucket_ptr;
  class SystemBucket;                                                // predeclaration
  typedef boost::shared_ptr< SystemBucket >     SystemBucket_ptr;
  class FunctionBucket;                                              // predeclaration
  typedef boost::shared_ptr< FunctionBucket >   FunctionBucket_ptr;
  class SolverBucket;                                                // predeclaration
  typedef boost::shared_ptr< SolverBucket >     SolverBucket_ptr;
  class GenericDetectors;                                            // predeclaration
  typedef boost::shared_ptr< GenericDetectors > GenericDetectors_ptr;
  class ReferencePoints;                                             // predeclaration
  typedef boost::shared_ptr< ReferencePoints >  ReferencePoints_ptr;

  //*****************************************************************|************************************************************//
  // boost shared pointers to dolfin objects
  //*****************************************************************|************************************************************//

  typedef boost::shared_ptr< dolfin::GenericFunction >              GenericFunction_ptr;
  typedef boost::shared_ptr< dolfin::Constant >                     Constant_ptr;
  typedef boost::shared_ptr< dolfin::Expression >                   Expression_ptr;
  typedef boost::shared_ptr< dolfin::Mesh >                         Mesh_ptr;
  typedef boost::shared_ptr< dolfin::MeshFunction< std::size_t > >  MeshFunction_size_t_ptr;
  typedef boost::shared_ptr< const dolfin::MeshFunction< std::size_t > > const_MeshFunction_size_t_ptr;
  typedef boost::shared_ptr< dolfin::FunctionSpace >                FunctionSpace_ptr;
  typedef boost::shared_ptr< dolfin::Function >                     Function_ptr;
  typedef boost::shared_ptr< dolfin::DirichletBC >                  DirichletBC_ptr;
  typedef boost::shared_ptr< dolfin::Form >                         Form_ptr;
  typedef boost::shared_ptr< dolfin::PETScMatrix >                  PETScMatrix_ptr;
  typedef boost::shared_ptr< dolfin::PETScVector >                  PETScVector_ptr;
  typedef boost::shared_ptr< dolfin::File >                         File_ptr;
  typedef boost::shared_ptr< dolfin::Array<double> >                Array_double_ptr;

  //*****************************************************************|************************************************************//
  // boost shared pointers to petsc objects
  //*****************************************************************|************************************************************//

  typedef boost::shared_ptr< KSP > KSP_ptr;
  typedef boost::shared_ptr< PC >  PC_ptr;
  typedef boost::shared_ptr< IS >  IS_ptr;
  typedef boost::shared_ptr< Mat > Mat_ptr;
  typedef boost::shared_ptr< Vec > Vec_ptr;

  //*****************************************************************|************************************************************//
  // iterators to boost shared pointers in map pointer structures
  //*****************************************************************|************************************************************//

  typedef std::map< std::string, SystemBucket_ptr >::iterator            SystemBucket_it;
  typedef std::map< std::string, SystemBucket_ptr >::const_iterator      SystemBucket_const_it;
  typedef std::map< int, SystemBucket_ptr >::iterator                    int_SystemBucket_it;
  typedef std::map< int, SystemBucket_ptr >::const_iterator              int_SystemBucket_const_it;
  typedef std::map< std::string, Mesh_ptr >::iterator                    Mesh_it;
  typedef std::map< std::string, Mesh_ptr >::const_iterator              Mesh_const_it;
  typedef std::map< std::string, std::string >::iterator                 string_it;
  typedef std::map< std::string, std::string >::const_iterator           string_const_it;
  typedef std::map< std::string, FunctionSpace_ptr >::iterator           FunctionSpace_it;
  typedef std::map< std::string, FunctionSpace_ptr >::const_iterator     FunctionSpace_const_it;
  typedef std::map< std::string, FunctionBucket_ptr >::iterator          FunctionBucket_it;
  typedef std::map< std::string, FunctionBucket_ptr >::const_iterator    FunctionBucket_const_it;
  typedef std::map< int, FunctionBucket_ptr >::iterator                  int_FunctionBucket_it;
  typedef std::map< int, FunctionBucket_ptr >::const_iterator            int_FunctionBucket_const_it;
  typedef std::map< std::string, SolverBucket_ptr >::iterator            SolverBucket_it;
  typedef std::map< std::string, SolverBucket_ptr >::const_iterator      SolverBucket_const_it;
  typedef std::map< int, SolverBucket_ptr >::iterator                    int_SolverBucket_it;
  typedef std::map< int, SolverBucket_ptr >::const_iterator              int_SolverBucket_const_it;
  typedef std::map< std::string, GenericFunction_ptr >::iterator         GenericFunction_it;
  typedef std::map< std::string, GenericFunction_ptr >::const_iterator   GenericFunction_const_it;
  typedef std::map< std::string, Function_ptr >::iterator                Function_it;
  typedef std::map< std::string, Function_ptr >::const_iterator          Function_const_it;
  typedef std::map< std::string, Expression_ptr >::iterator              Expression_it;
  typedef std::map< std::string, Expression_ptr >::const_iterator        Expression_const_it;
  typedef std::map< std::string, DirichletBC_ptr >::iterator             DirichletBC_it;
  typedef std::map< std::string, DirichletBC_ptr >::const_iterator       DirichletBC_const_it;
  typedef std::map< int, DirichletBC_ptr >::iterator                     int_DirichletBC_it;
  typedef std::map< int, DirichletBC_ptr >::const_iterator               int_DirichletBC_const_it;
  typedef std::map< std::string, ReferencePoints_ptr >::iterator         ReferencePoints_it;
  typedef std::map< std::string, ReferencePoints_ptr >::const_iterator   ReferencePoints_const_it;
  typedef std::map< std::size_t, Expression_ptr >::iterator              size_t_Expression_it;
  typedef std::map< std::size_t, Expression_ptr >::const_iterator        size_t_Expression_const_it;
  typedef std::map< int, FunctionSpace_ptr >::iterator                   int_FunctionSpace_it;
  typedef std::map< int, FunctionSpace_ptr >::const_iterator             int_FunctionSpace_const_it;
  typedef std::map< std::string, Form_ptr >::iterator                    Form_it;
  typedef std::map< std::string, Form_ptr >::const_iterator              Form_const_it;
  typedef std::map< std::string, GenericDetectors_ptr >::iterator        GenericDetectors_it;
  typedef std::map< std::string, GenericDetectors_ptr >::const_iterator  GenericDetectors_const_it;
  typedef std::map< std::string, double_ptr >::iterator                  double_ptr_it;
  typedef std::map< std::string, double_ptr >::const_iterator            double_ptr_const_it;
  typedef std::map< std::string, bool_ptr >::iterator                    bool_ptr_it;
  typedef std::map< std::string, bool_ptr >::const_iterator              bool_ptr_const_it;
  
}

#endif
