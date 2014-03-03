// Copyright (C) 2013 Columbia University in the City of New York and others.
//
// Please see the AUTHORS file in the main source directory for a full list
// of contributors.
//
// This file is part of TerraFERMA.
//
// TerraFERMA is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TerraFERMA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with TerraFERMA. If not, see <http://www.gnu.org/licenses/>.


#include "SolverBucket.h"
#include "BoostTypes.h"
#include "SystemBucket.h"
#include "Bucket.h"
#include "SignalHandler.h"
#include <dolfin.h>
#include <string>
#include <signal.h>

using namespace buckettools;

//*******************************************************************|************************************************************//
// default constructor
//*******************************************************************|************************************************************//
SolverBucket::SolverBucket()
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
SolverBucket::SolverBucket(SystemBucket* system) : system_(system)
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// default destructor
//*******************************************************************|************************************************************//
SolverBucket::~SolverBucket()
{
  empty_();                                                          // empty the solver bucket data structures

  PetscErrorCode perr;                                               // petsc error code

  if(type()=="SNES" && !copy_)
  {
    #if PETSC_VERSION_MAJOR == 3 && PETSC_VERSION_MINOR > 1
    perr = SNESDestroy(&snes_); CHKERRV(perr);                        // destroy the snes object
    #else
    perr = SNESDestroy(snes_); CHKERRV(perr);                         // destroy the snes object
    #endif
  }

  if(type()=="Picard" && !copy_)
  {
    #if PETSC_VERSION_MAJOR == 3 && PETSC_VERSION_MINOR > 1
    perr = KSPDestroy(&ksp_); CHKERRV(perr);                          // destroy the ksp object
    #else
    perr = KSPDestroy(ksp_); CHKERRV(perr);                          // destroy the ksp object
    #endif
  }

}

//*******************************************************************|************************************************************//
// solve the bilinear system described by the forms in the solver bucket
//*******************************************************************|************************************************************//
void SolverBucket::solve()
{
  PetscErrorCode perr;

  if ((*system_).solve_location()==SOLVE_NEVER)
  {
    dolfin::error("Unable to solve as solve_location set to never.");
  }

  dolfin::log(dolfin::INFO, "Solving for %s::%s using %s", 
                          (*system_).name().c_str(), name().c_str(), 
                          type().c_str());

  if (type()=="SNES")                                                // this is a petsc snes solver - FIXME: switch to an enumerated type
  {
    for(std::vector< const dolfin::DirichletBC* >::const_iterator    // loop over the collected vector of system bcs
                      bc = (*system_).dirichletbcs_begin(); 
                      bc != (*system_).dirichletbcs_end(); bc++)
    {
      (*(*bc)).apply(*(*(*system_).function()).vector());            // apply the bcs to the solution and
      (*(*bc)).apply(*(*(*system_).iteratedfunction()).vector());    // iterated solution
    }
    *work_ = (*(*(*system_).function()).vector());                   // set the work vector to the function vector
    perr = SNESSolve(snes_, PETSC_NULL, (*work_).vec());            // call petsc to perform a snes solve
    if (perr>0)
    {
      dolfin::log(dolfin::ERROR, "ERROR: SNESSolve returned error code %d.", perr);
      (*SignalHandler::instance()).dispatcher(SIGINT);
    }
    CHKERRV(perr);
    snes_check_convergence_();
    (*(*(*system_).function()).vector()) = *work_;                   // update the function
  }
  else if (type()=="Picard")                                         // this is a hand-rolled picard iteration - FIXME: switch to enum
  {

    *iteration_count_ = 0;                                           // an iteration counter

    File_ptr pvdfile;
    FunctionSpace_ptr visfuncspace;
    std::vector< GenericFunction_ptr > functions;
    if(*visualizationmonitor_)
    {
      std::stringstream buffer;
      buffer.str(""); buffer << (*(*system()).bucket()).output_basename() << "_" 
                             << (*system()).name() << "_" 
                             << name() << "_" 
                             << (*(*system()).bucket()).timestep_count() << "_" 
                             << (*(*system()).bucket()).iteration_count() << "_picard.pvd";
      pvdfile.reset( new dolfin::File(buffer.str(), "compressed") );

      Mesh_ptr sysmesh = (*system()).mesh();
      visfuncspace = (*(*system()).bucket()).fetch_visfunctionspace(sysmesh);

      for (FunctionBucket_const_it f_it = (*system()).fields_begin(); 
                                   f_it != (*system()).fields_end(); 
                                                            f_it++)
      {
        functions.push_back((*(*f_it).second).iteratedfunction());
        functions.push_back((*(*f_it).second).residualfunction());
      }

    }

    assert(residual_);                                               // we need to assemble the residual again here as it may depend
                                                                     // on other systems that have been solved since the last call
    dolfin::Assembler assemblerres;
    assemblerres.reset_sparsity = false;
    assemblerres.assemble(*res_, *residual_);                        // assemble the residual
    for(std::vector< const dolfin::DirichletBC* >::const_iterator bc = 
                          (*system_).dirichletbcs_begin(); 
                          bc != (*system_).dirichletbcs_end(); bc++)
    {                                                                // apply bcs to residuall (should we do this?!)
      (*(*bc)).apply(*res_, (*(*(*system_).iteratedfunction()).vector()));
    }

    for(std::vector<ReferencePoints_ptr>::const_iterator p = 
                                    (*system_).points_begin(); 
                                p != (*system_).points_end(); p++)
    {                                                                // apply reference points to residual (should we do this?!)
      (*(*p)).apply(*res_, (*(*(*system_).iteratedfunction()).vector()));
    }

    double aerror = (*res_).norm("l2");                              // work out the initial absolute l2 error (this should be
                                                                     // initialized to the right value on the first pass and still
                                                                     // be the correct value from the previous sweep (if this stops
                                                                     // being the case it will be necessary to assemble the residual
                                                                     // here too)
    double aerror0 = aerror;                                         // record the initial absolute error
    double rerror;
    if(aerror==0.0)
    {
      rerror = aerror;                                               // relative error, starts out as 0 - won't iterate at all
    }
    else
    {
      rerror = aerror/aerror0;                                       // relative error, starts out as 1.
    }

    dolfin::info("  %u Picard Residual Norm (absolute, relative) = %g, %g\n", 
                                    iteration_count(), aerror, rerror);

    if(*visualizationmonitor_ || convfile_)
    {
      *(*(*system()).residualfunction()).vector() = (*std::dynamic_pointer_cast< dolfin::GenericVector >(residual_vector()));
      if (*visualizationmonitor_)
      {
        (*pvdfile).write(functions, *visfuncspace, (double) iteration_count());
      }
      if (convfile_)
      {
        (*convfile_).write_data();
      }
    }


    (*(*(*system_).iteratedfunction()).vector()) =                   // system iterated function gets set to the function values
                                (*(*(*system_).function()).vector());

    while (iteration_count() < minits_ ||                            // loop for the minimum number of iterations or
          (iteration_count() < maxits_ &&                            // up to the maximum number of iterations 
                           rerror > rtol_ && aerror > atol_))        // until the max is reached or a tolerance criterion is
    {                                                                // satisfied
      (*iteration_count_)++;                                         // increment iteration counter

      dolfin::SystemAssembler assembler(bilinear_, linear_,
                                        (*system_).dirichletbcs());
      assembler.reset_sparsity = false;
      assembler.assemble(*matrix_, *rhs_);

      for(std::vector<ReferencePoints_ptr>::const_iterator p =       // loop over the collected vector of system reference points
                                      (*system_).points_begin(); 
                                  p != (*system_).points_end(); p++)
      {
        (*(*p)).apply(*matrix_, *rhs_);                              // apply the reference points to the matrix and rhs
      }

      if(ident_zeros_)
      {
        (*matrix_).ident_zeros();
      }

      if (bilinearpc_)                                               // if there's a pc associated
      {
        assert(matrixpc_);
        dolfin::SystemAssembler assemblerpc(bilinearpc_, linear_,
                                          (*system_).dirichletbcs());
        assemblerpc.reset_sparsity = false;
        assemblerpc.assemble(*matrixpc_);

        for(std::vector<ReferencePoints_ptr>::const_iterator p =     // loop over the collected vector of system reference points
                                        (*system_).points_begin(); 
                                    p != (*system_).points_end(); p++)
        {
          (*(*p)).apply(*matrixpc_);                                 // apply the reference points to the pc matrix
        }

        if(ident_zeros_pc_)
        {
          (*matrixpc_).ident_zeros();
        }

        perr = KSPSetOperators(ksp_, (*matrix_).mat(),              // set the ksp operators with two matrices
                                     (*matrixpc_).mat(), 
                                     SAME_NONZERO_PATTERN); 
        CHKERRV(perr);
      }
      else
      {
        perr = KSPSetOperators(ksp_, (*matrix_).mat(),              // set the ksp operators with the same matrices
                                      (*matrix_).mat(), 
                                        SAME_NONZERO_PATTERN); 
        CHKERRV(perr);
      }

      for (Form_const_it f_it = solverforms_begin(); 
                         f_it != solverforms_end(); f_it++)
      {
        PETScMatrix_ptr solvermatrix = solvermatrices_[(*f_it).first];
        dolfin::SystemAssembler assemblerform((*f_it).second, linear_,
                                          (*system_).dirichletbcs());
        assemblerform.reset_sparsity = false;
        assemblerform.assemble(*solvermatrix);

        for(std::vector<ReferencePoints_ptr>::const_iterator p =     // loop over the collected vector of system reference points
                                        (*system_).points_begin(); 
                                    p != (*system_).points_end(); p++)
        {
          (*(*p)).apply(*solvermatrix);                              // apply the reference points to the pc matrix
        }

        if(solverident_zeros_[(*f_it).first])
        {
          (*solvermatrix).ident_zeros();
        }

        IS_ptr is = solverindexsets_[(*f_it).first];
        Mat_ptr submatrix = solversubmatrices_[(*f_it).first];
        perr = MatGetSubMatrix((*solvermatrix).mat(), *is, *is, MAT_REUSE_MATRIX, &(*submatrix));
        CHKERRV(perr);

      }

      if (monitor_norms())
      {
        PetscReal norm;

        perr = VecNorm((*rhs_).vec(),NORM_2,&norm); CHKERRV(perr);
        dolfin::log(dolfin::get_log_level(), "Picard: 2-norm rhs = %f", norm);

        perr = VecNorm((*rhs_).vec(),NORM_INFINITY,&norm); CHKERRV(perr);
        dolfin::log(dolfin::get_log_level(), "Picard: inf-norm rhs = %f", norm);

        perr = VecNorm((*work_).vec(),NORM_2,&norm); CHKERRV(perr);
        dolfin::log(dolfin::get_log_level(), "Picard: 2-norm work = %f", norm);

        perr = VecNorm((*work_).vec(),NORM_INFINITY,&norm); CHKERRV(perr);
        dolfin::log(dolfin::get_log_level(), "Picard: inf-norm work = %f", norm);

        perr = MatNorm((*matrix_).mat(),NORM_FROBENIUS,&norm); CHKERRV(perr);
        dolfin::log(dolfin::get_log_level(), "Picard: Frobenius norm matrix = %f", norm);

        perr = MatNorm((*matrix_).mat(),NORM_INFINITY,&norm); CHKERRV(perr);
        dolfin::log(dolfin::get_log_level(), "Picard: inf-norm matrix = %f", norm);

        if (bilinearpc_)
        {
          perr = MatNorm((*matrixpc_).mat(),NORM_FROBENIUS,&norm); CHKERRV(perr);
          dolfin::log(dolfin::get_log_level(), "Picard: Frobenius norm matrix pc = %f", norm);

          perr = MatNorm((*matrixpc_).mat(),NORM_INFINITY,&norm); CHKERRV(perr);
          dolfin::log(dolfin::get_log_level(), "Picard: inf-norm matrix pc = %f", norm);
        }
      }

      *work_ = (*(*(*system_).iteratedfunction()).vector());         // set the work vector to the iterated function
      perr = KSPSolve(ksp_, (*rhs_).vec(), (*work_).vec());        // perform a linear solve
      CHKERRV(perr);
      ksp_check_convergence_(ksp_);
      (*(*(*system_).iteratedfunction()).vector()) = *work_;         // update the iterated function with the work vector

      assert(residual_);
      assemblerres.assemble(*res_, *residual_);                      // assemble the residual
      for(std::vector< const dolfin::DirichletBC* >::const_iterator bc = 
                             (*system_).dirichletbcs_begin(); 
                             bc != (*system_).dirichletbcs_end(); bc++)
      {                                                              // apply bcs to residual (should we do this?!)
        (*(*bc)).apply(*res_, (*(*(*system_).iteratedfunction()).vector()));
      }
      for(std::vector<ReferencePoints_ptr>::const_iterator p = 
                                      (*system_).points_begin(); 
                                  p != (*system_).points_end(); p++)
      {                                                              // apply reference points to residual (should we do this?!)
        (*(*p)).apply(*res_, (*(*(*system_).iteratedfunction()).vector()));
      }

      aerror = (*res_).norm("l2");                                   // work out absolute error
      rerror = aerror/aerror0;                                       // and relative error
      dolfin::info("  %u Picard Residual Norm (absolute, relative) = %g, %g\n", 
                          iteration_count(), aerror, rerror);
                                                                     // and decide to loop or not...

      if(*visualizationmonitor_ || convfile_)
      {
        *(*(*system()).residualfunction()).vector() = (*std::dynamic_pointer_cast< dolfin::GenericVector >(residual_vector()));
        if (*visualizationmonitor_)
        {
          (*pvdfile).write(functions, *visfuncspace, (double) iteration_count());
        }
        if (convfile_)
        {
          (*convfile_).write_data();
        }
      }

    }

    if (iteration_count() == maxits_ && rerror > rtol_ && aerror > atol_)
    {
      dolfin::log(dolfin::WARNING, "it = %d, maxits_ = %d", iteration_count(), maxits_);
      dolfin::log(dolfin::WARNING, "rerror = %f, rtol_ = %f", rerror, rtol_);
      dolfin::log(dolfin::WARNING, "aerror = %f, atol_ = %f", aerror, atol_);
      if (ignore_failures_)
      {
        dolfin::log(dolfin::WARNING, "Picard iterations failed to converge, ignoring.");
      }
      else
      {
        dolfin::log(dolfin::ERROR, "Picard iterations failed to converge, sending sig int.");
        (*SignalHandler::instance()).dispatcher(SIGINT);
      }
    }

    (*(*(*system_).function()).vector()) =                              // update the function values with the iterated values
                      (*(*(*system_).iteratedfunction()).vector());

  }
  else                                                               // don't know what solver type this is
  {
    dolfin::error("Unknown solver type.");
  }

}

//*******************************************************************|************************************************************//
// assemble all linear forms (this includes initializing the vectors if necessary)
//*******************************************************************|************************************************************//
void SolverBucket::assemble_linearforms()
{
  assert(linear_);
  dolfin::Assembler assembler;
  assembler.reset_sparsity=false;

  assembler.assemble(*rhs_, *linear_);

  if(residual_)                                                      // do we have a residual_ form?
  {                                                                  // yes...
    assembler.assemble(*res_, *residual_);                           // and assemble it
  }
}

//*******************************************************************|************************************************************//
// assemble all bilinear forms (this includes initializing the matrices if necessary)
//*******************************************************************|************************************************************//
void SolverBucket::assemble_bilinearforms()
{
  assert(bilinear_);
  dolfin::SystemAssembler assembler(bilinear_, linear_, 
                                    (*system_).dirichletbcs());
  assembler.reset_sparsity=false;
  assembler.assemble(*matrix_);

  if(bilinearpc_)                                                    // do we have a pc form?
  {
    dolfin::SystemAssembler assemblerpc(bilinearpc_, linear_,
                                      (*system_).dirichletbcs());
    assemblerpc.reset_sparsity=false;
    assemblerpc.assemble(*matrixpc_);
  }

  for (Form_const_it f_it = solverforms_begin(); 
                     f_it != solverforms_end(); f_it++)
  {
    PETScMatrix_ptr solvermatrix = solvermatrices_[(*f_it).first];
    dolfin::SystemAssembler assemblerform((*f_it).second, linear_,
                                      (*system_).dirichletbcs());
    assemblerform.reset_sparsity=false;
    assemblerform.assemble(*solvermatrix);
  }

}

//*******************************************************************|************************************************************//
// loop over the forms in this solver bucket and attach the coefficients they request using the parent bucket data maps
//*******************************************************************|************************************************************//
void SolverBucket::attach_form_coeffs()
{
  (*(*system_).bucket()).attach_coeffs(forms_begin(), forms_end());
}

//*******************************************************************|************************************************************//
// make a partial copy of the provided solver bucket with the data necessary for writing the diagnostics file(s)
//*******************************************************************|************************************************************//
void SolverBucket::copy_diagnostics(SolverBucket_ptr &solver, SystemBucket_ptr &system) const
{

  if(!solver)
  {
    solver.reset( new SolverBucket(&(*system)) );
  }

  (*solver).iteration_count_ = iteration_count_;
  (*solver).name_ = name_;
  (*solver).type_ = type_;   
  (*solver).copy_ = true;                                            // this is done to ensure that the petsc destroy routines
                                                                     // are not called by the destructor

}

//*******************************************************************|************************************************************//
// initialize any diagnostic output from the solver
//*******************************************************************|************************************************************//
void SolverBucket::initialize_diagnostics() const                    // doesn't allocate anything so can be const
{
  if (convfile_)
  {
    (*convfile_).write_header((*(*system()).bucket()));
  }
  if (kspconvfile_)
  {
    (*kspconvfile_).write_header((*(*system()).bucket()));
  }
}

//*******************************************************************|************************************************************//
// return the number of nonlinear iterations taken
//*******************************************************************|************************************************************//
const int SolverBucket::iteration_count() const
{
  return *iteration_count_;
}

//*******************************************************************|************************************************************//
// set the number of nonlinear iterations taken
//*******************************************************************|************************************************************//
void SolverBucket::iteration_count(const int &it)
{
  *iteration_count_ = it;
}

//*******************************************************************|************************************************************//
// return true if we're using a visualization monitor
//*******************************************************************|************************************************************//
const bool SolverBucket::visualization_monitor() const
{
  return *visualizationmonitor_;
}

//*******************************************************************|************************************************************//
// return true if we're using a ksp visualization monitor
//*******************************************************************|************************************************************//
const bool SolverBucket::kspvisualization_monitor() const
{
  return *kspvisualizationmonitor_;
}

// return a pointer to the convergence file
//*******************************************************************|************************************************************//
const ConvergenceFile_ptr SolverBucket::convergence_file() const
{
  return convfile_;
}

//*******************************************************************|************************************************************//
// return a pointer to the ksp convergence file
//*******************************************************************|************************************************************//
const KSPConvergenceFile_ptr SolverBucket::ksp_convergence_file() const
{
  return kspconvfile_;
}

//*******************************************************************|************************************************************//
// register a (boost shared) pointer to a form in the solver bucket data maps
//*******************************************************************|************************************************************//
void SolverBucket::register_form(Form_ptr form, const std::string &name)
{
  Form_it f_it = forms_.find(name);                                  // check if this name already exists
  if (f_it != forms_.end())
  {
    dolfin::error("Form named \"%s\" already exists in solver.",     // if it does, issue an error
                                                  name.c_str());
  }
  else
  {
    forms_[name] = form;                                             // if not, register the form in the maps
  }
}

//*******************************************************************|************************************************************//
// return a boolean indicating if the solver bucket contains a form with the given name
//*******************************************************************|************************************************************//
bool SolverBucket::contains_form(const std::string &name)                   
{
  Form_it f_it = forms_.find(name);
  return f_it != forms_.end();
}

//*******************************************************************|************************************************************//
// return a (boost shared) pointer to a form from the solver bucket data maps
//*******************************************************************|************************************************************//
Form_ptr SolverBucket::fetch_form(const std::string &name)
{
  Form_it f_it = forms_.find(name);                                  // check if this name already exists
  if (f_it == forms_.end())
  {
    dolfin::error("Form named \"%s\" does not exist in solver.",     // if it doesn't, issue an error
                                                    name.c_str());
  }
  else
  {
    return (*f_it).second;                                           // if it does, return it
  }
}

//*******************************************************************|************************************************************//
// return an iterator to the beginning of the forms_ map
//*******************************************************************|************************************************************//
Form_it SolverBucket::forms_begin()
{
  return forms_.begin();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the beginning of the forms_ map
//*******************************************************************|************************************************************//
Form_const_it SolverBucket::forms_begin() const
{
  return forms_.begin();
}

//*******************************************************************|************************************************************//
// return an iterator to the end of the forms_ map
//*******************************************************************|************************************************************//
Form_it SolverBucket::forms_end()
{
  return forms_.end();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the end of the forms_ map
//*******************************************************************|************************************************************//
Form_const_it SolverBucket::forms_end() const
{
  return forms_.end();
}

//*******************************************************************|************************************************************//
// register a (boost shared) pointer to a form in the solver bucket data maps
//*******************************************************************|************************************************************//
void SolverBucket::register_solverform(Form_ptr form, const std::string &name)
{
  Form_it f_it = solverforms_.find(name);                            // check if this name already exists
  if (f_it != solverforms_.end())
  {
    dolfin::error("Solver form named \"%s\" already exists in solver.",     // if it does, issue an error
                                                  name.c_str());
  }
  else
  {
    solverforms_[name] = form;                                             // if not, register the form in the maps
  }
}

//*******************************************************************|************************************************************//
// return an iterator to the beginning of the solverforms_ map
//*******************************************************************|************************************************************//
Form_it SolverBucket::solverforms_begin()
{
  return solverforms_.begin();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the beginning of the solverforms_ map
//*******************************************************************|************************************************************//
Form_const_it SolverBucket::solverforms_begin() const
{
  return solverforms_.begin();
}

//*******************************************************************|************************************************************//
// return an iterator to the end of the solverforms_ map
//*******************************************************************|************************************************************//
Form_it SolverBucket::solverforms_end()
{
  return solverforms_.end();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the end of the solverforms_ map
//*******************************************************************|************************************************************//
Form_const_it SolverBucket::solverforms_end() const
{
  return solverforms_.end();
}

//*******************************************************************|************************************************************//
// return a (boost shared) pointer to a petsc matrix from the solver bucket data maps
//*******************************************************************|************************************************************//
PETScMatrix_ptr SolverBucket::fetch_solvermatrix(const std::string &name)
{
  std::map< std::string, PETScMatrix_ptr>::iterator s_it = 
                                          solvermatrices_.find(name);// check if this name already exists
  if (s_it == solvermatrices_.end())
  {
    dolfin::error("Solver matrix named \"%s\" does not exist in solver.",     // if it doesn't, issue an error
                                                    name.c_str());
  }
  else
  {
    return (*s_it).second;                                           // if it does, return it
  }
}

//*******************************************************************|************************************************************//
// return a (boost shared) pointer to an index set for this solver sub matrix
//*******************************************************************|************************************************************//
IS_ptr SolverBucket::fetch_solverindexset(const std::string &name)
{
  std::map< std::string, IS_ptr >::iterator s_it = 
                                         solverindexsets_.find(name);// check if this name already exists
  if (s_it == solverindexsets_.end())
  {
    dolfin::error("Solver index set named \"%s\" does not exist in solver.",     // if it doesn't, issue an error
                                                    name.c_str());
  }
  else
  {
    return (*s_it).second;                                           // if it does, return it
  }
}

//*******************************************************************|************************************************************//
// return a bool indicating if the named solver form/matrix should have zeros idented
//*******************************************************************|************************************************************//
bool SolverBucket::solverident_zeros(const std::string &name)
{
  std::map< std::string, bool >::iterator s_it = 
                                       solverident_zeros_.find(name);// check if this name already exists
  if (s_it == solverident_zeros_.end())
  {
    dolfin::error("Solver ident zeros named \"%s\" does not exist in solver.",     // if it doesn't, issue an error
                                                    name.c_str());
  }
  else
  {
    return (*s_it).second;                                           // if it does, return it
  }
}

//*******************************************************************|************************************************************//
// return a (boost shared) pointer to a petsc matrix from the solver bucket data maps
//*******************************************************************|************************************************************//
Mat_ptr SolverBucket::fetch_solversubmatrix(const std::string &name)
{
  std::map< std::string, Mat_ptr >::iterator s_it = 
                                          solversubmatrices_.find(name);// check if this name already exists
  if (s_it == solversubmatrices_.end())
  {
    dolfin::error("Solver sub matrix named \"%s\" does not exist in solver.",     // if it doesn't, issue an error
                                                    name.c_str());
  }
  else
  {
    return (*s_it).second;                                           // if it does, return it
  }
}

//*******************************************************************|************************************************************//
// return a string describing the contents of the solver bucket
//*******************************************************************|************************************************************//
const std::string SolverBucket::str(int indent) const
{
  std::stringstream s;
  std::string indentation (indent*2, ' ');
  s << indentation << "SolverBucket " << name() << std::endl;
  indent++;
  s << forms_str(indent);
  return s.str();
}

//*******************************************************************|************************************************************//
// return a string describing the forms in the solver bucket
//*******************************************************************|************************************************************//
const std::string SolverBucket::forms_str(const int &indent) const
{
  std::stringstream s;
  std::string indentation (indent*2, ' ');
  for ( Form_const_it f_it = forms_.begin(); f_it != forms_.end(); f_it++ )
  {
    s << indentation << "Form " << (*f_it).first  << std::endl;
  }
  return s.str();
}

//*******************************************************************|************************************************************//
// report the convergence of the snes solver
//*******************************************************************|************************************************************//
void SolverBucket::snes_check_convergence_()
{
  PetscErrorCode perr;                                               // petsc error code

  assert(type()=="SNES");

  dolfin::log(dolfin::INFO, "Convergence for %s::%s", 
                          (*system_).name().c_str(), name().c_str());

  SNESConvergedReason snesreason;                                    // check what the convergence reason was
  PetscInt snesiterations;
  PetscInt sneslsiterations;
//  const char **snesprefix;
//  perr = SNESGetOptionsPrefix(snes_, snesprefix); CHKERRV(perr);   // FIXME: segfaults!
  perr = SNESGetConvergedReason(snes_, &snesreason); CHKERRV(perr);     
  perr = SNESGetIterationNumber(snes_, &snesiterations); CHKERRV(perr);
  perr = SNESGetLinearSolveIterations(snes_, &sneslsiterations);  CHKERRV(perr);
  dolfin::log(dolfin::INFO, "SNESConvergedReason %d", snesreason);
  dolfin::log(dolfin::INFO, "SNES n/o iterations %d", 
                              snesiterations);
  dolfin::log(dolfin::INFO, "SNES n/o linear solver iterations %d", 
                              sneslsiterations);
  if (snesreason<0)
  {
    if (ignore_failures_)
    {
      dolfin::log(dolfin::WARNING, "SNESConvergedReason <= 0, ignoring.");
    }
    else
    {
      dolfin::log(dolfin::ERROR, "SNESConvergedReason <= 0, sending sig int.");
      (*SignalHandler::instance()).dispatcher(SIGINT);
    }
  }

  ksp_check_convergence_(ksp_, 1);

}

//*******************************************************************|************************************************************//
// report the convergence of a ksp solver
//*******************************************************************|************************************************************//
void SolverBucket::ksp_check_convergence_(KSP &ksp, int indent)
{
  PetscErrorCode perr;                                               // petsc error code
  std::string indentation (indent*2, ' ');

  if (indent==0)
  {
    dolfin::log(dolfin::INFO, "Convergence for %s::%s", 
                          (*system_).name().c_str(), name().c_str());
  }

  KSPConvergedReason kspreason;                                      // check what the convergence reason was
  PetscInt kspiterations;
  //const char **kspprefix;
  //perr = KSPGetOptionsPrefix(ksp, kspprefix); CHKERRV(perr);       // FIXME: segfaults!
  perr = KSPGetConvergedReason(ksp, &kspreason); CHKERRV(perr);     
  perr = KSPGetIterationNumber(ksp, &kspiterations); CHKERRV(perr);     
  dolfin::log(dolfin::INFO, "%sKSPConvergedReason %d", 
                              indentation.c_str(), kspreason);
  dolfin::log(dolfin::INFO, "%sKSP n/o iterations %d", 
                              indentation.c_str(), kspiterations);
  if (indent==0 && kspreason<0)
  {
    if (ignore_failures_)
    {
      dolfin::log(dolfin::WARNING, "KSPConvergedReason <= 0, ignoring.");
    }
    else
    {
      dolfin::log(dolfin::ERROR, "KSPConvergedReason <= 0, sending sig int.");
      (*SignalHandler::instance()).dispatcher(SIGINT);
    }
  }


  indent++;

  PC pc;
  perr = KSPGetPC(ksp, &pc); CHKERRV(perr);
  #if PETSC_VERSION_MAJOR == 3 && PETSC_VERSION_MINOR > 3
  PCType pctype;
  #else
  const PCType pctype;
  #endif
  perr = PCGetType(pc, &pctype); CHKERRV(perr);

  if ((std::string)pctype=="ksp")
  {
    KSP subksp;                                                      // get the subksp from this pc
    perr = PCKSPGetKSP(pc, &subksp); CHKERRV(perr);
    ksp_check_convergence_(subksp, indent);
  }
  else if ((std::string)pctype=="fieldsplit")
  {
    KSP *subksps;                                                    // get the fieldsplit subksps
    PetscInt nsubksps;
    perr = PCFieldSplitGetSubKSP(pc, &nsubksps, &subksps); 
    CHKERRV(perr); 
    for (PetscInt i = 0; i < nsubksps; i++)
    {
      ksp_check_convergence_(subksps[i], indent);
    }
  }
  
}

//*******************************************************************|************************************************************//
// empty the data structures in the solver bucket
//*******************************************************************|************************************************************//
void SolverBucket::empty_()
{
  forms_.clear();
}

