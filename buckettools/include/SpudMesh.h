#ifndef __SPUD_MESH_H
#define __SPUD_MESH_H

#include <dolfin.h>

namespace buckettools
{
  
  // A simple class to supplement some spud specific members to the dolfin Mesh class
  class SpudMesh : public dolfin::Mesh
  {
  private:
    
    // A name (not really spud specific but a nice feature)
    std::string name_;
    // The option path for this mesh in the spud dictionary
    std::string option_path_;
    
  public:
    
    SpudMesh(std::string filename)
    { SpudMesh("uninitialised_name", "uninitialised_path", filename); }
    
    SpudMesh(std::string name, std::string option_path, filename);
    
    virtual ~SpudMesh();
    
  };

  // Define a boost shared_ptr for the SpudMesh class
  typedef boost::shared_ptr< SpudMesh > SpudMesh_ptr;
  
}
#endif