// This file is part of Hermes2D.
//
// Hermes2D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Hermes2D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Hermes2D.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __H2D_ORDERIZER_H
#define __H2D_ORDERIZER_H

#include "linearizer.h"

namespace Hermes
{
  namespace Hermes2D
  {
    namespace Views
    {
      /// Like the Linearizer, but generates a triangular mesh showing polynomial
      /// orders in a space, hence the funky name.
      ///
      class HERMES_API Orderizer : public LinearizerBase
      {
      public:

        Orderizer();
        ~Orderizer();

        /// Saves the polynomial orders.
        template<typename Scalar>
        void save_orders_vtk(SpaceSharedPtr<Scalar> space, const char* file_name);

        /// Saves the mesh with markers.
        template<typename Scalar>
        void save_markers_vtk(SpaceSharedPtr<Scalar> space, const char* file_name);

        /// Saves the mesh - edges.
        template<typename Scalar>
        void save_mesh_vtk(SpaceSharedPtr<Scalar> space, const char* file_name);

        /// Returns axis aligned bounding box (AABB) of vertices. Assumes lock.
        void calc_vertices_aabb(double* min_x, double* max_x,
          double* min_y, double* max_y) const;

        /// Internal.
        template<typename Scalar>
        void process_space(SpaceSharedPtr<Scalar> space, bool show_edge_orders = false);

        int get_labels(int*& lvert, char**& ltext, double2*& lbox) const;

        int get_num_vertices();
        double3* get_vertices();

        void free();
      protected:
        /// Reallocation at the beginning of process_*.
        /// Specific for Linearizer
        void reallocate_specific(int number_of_elements);

        char  buffer[1000];
        char* labels[11][11];

        double3* verts;  ///< vertices: (x, y, value) triplets
        int  label_size, label_count;
        int* lvert;
        char** ltext;
        double2* lbox;

        void add_edge(int iv1, int iv2, int marker);

        int add_vertex();

        void make_vert(int & index, double x, double y, double val);
      };
    }
  }
}
#endif