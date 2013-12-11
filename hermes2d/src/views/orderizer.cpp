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

#include "orderizer.h"
#include "space.h"
#include "refmap.h"
#include "orderizer_quad.cpp"

namespace Hermes
{
  namespace Hermes2D
  {
    namespace Views
    {
      // vertices
      static int*      ord_np[2] = { num_vert_tri, num_vert_quad };
      static double3*  ord_tables_tri[2] = { vert_tri0, vert_tri1 };
      static double3*  ord_tables_quad[2] = { vert_quad0, vert_quad1 };
      static double3** ord_tables[2] = { ord_tables_tri, ord_tables_quad };

      // triangles
      static int*      num_elem[2] = { num_elem_tri, num_elem_quad };
      static int3*     ord_elem_tri[2] = { elem_tri0, elem_tri1 };
      static int3*     ord_elem_quad[2] = { elem_quad0, elem_quad1 };
      static int3**    ord_elem[2] = { ord_elem_tri, ord_elem_quad };

      // edges
      static int*      num_edge[2] = { num_edge_tri, num_edge_quad };
      static int3*     ord_edge_tri[2] = { edge_tri0, edge_tri1 };
      static int3*     ord_edge_quad[2] = { edge_quad0, edge_quad1 };
      static int3**    ord_edge[2] = { ord_edge_tri, ord_edge_quad };

      // vertices_simple
      static int*      ord_np_simple[2] = { num_vert_tri_simple, num_vert_quad_simple };
      static double3*  ord_tables_tri_simple[2] = { vert_tri_simple, vert_tri_simple };
      static double3*  ord_tables_quad_simple[2] = { vert_quad_simple, vert_quad_simple };
      static double3** ord_tables_simple[2] = { ord_tables_tri_simple, ord_tables_quad_simple };

      // triangles
      static int*      num_elem_simple[2] = { num_elem_tri_simple, num_elem_quad_simple };
      static int3*     ord_elem_tri_simple[2] = { elem_tri_simple, elem_tri_simple };
      static int3*     ord_elem_quad_simple[2] = { elem_quad_simple, elem_quad_simple };
      static int3**    ord_elem_simple[2] = { ord_elem_tri_simple, ord_elem_quad_simple };

      // edges
      static int*      num_edge_simple[2] = { num_edge_tri_simple, num_edge_quad_simple };
      static int3*     ord_edge_tri_simple[2] = { edge_tri_simple, edge_tri_simple };
      static int3*     ord_edge_quad_simple[2] = { edge_quad_simple, edge_quad_simple };
      static int3**    ord_edge_simple[2] = { ord_edge_tri_simple, ord_edge_quad_simple };

      static class Quad2DOrd : public Quad2D
      {
      public:

        Quad2DOrd()
        {
          max_order[0] = max_order[1] = 1;
          num_tables[0] = num_tables[1] = 2;
          tables = ord_tables;
          np = ord_np;
        };
      } quad_ord;

      static class Quad2DOrdSimple : public Quad2D
      {
      public:

        Quad2DOrdSimple()
        {
          max_order[0] = max_order[1] = 1;
          num_tables[0] = num_tables[1] = 2;
          tables = ord_tables_simple;
          np = ord_np_simple;
        };
      } quad_ord_simple;

      Orderizer::Orderizer() : LinearizerBase()
      {
        verts = nullptr;
        this->label_size = 0;
        ltext = nullptr;
        lvert = nullptr;
        lbox = nullptr;

        label_count = 0;

        for (int i = 0, p = 0; i <= 10; i++)
        {
          for (int j = 0; j <= 10; j++)
          {
            assert((unsigned)p < sizeof(buffer)-5);
            if (i == j)
              sprintf(buffer + p, "%d", i);
            else
              sprintf(buffer + p, "%d|%d", i, j);
            labels[i][j] = buffer + p;
            p += strlen(buffer + p) + 1;
          }
        }
      }

      int Orderizer::add_vertex()
      {
        if (this->vertex_count >= this->vertex_size)
        {
          void* new_verts = realloc(verts, sizeof(double3)* (this->vertex_size = this->vertex_size * 1.5));
          if(new_verts)
            verts = (double3*)new_verts;
          else
          {
            this->free();
            this->deinit_linearizer_base();
            throw Exceptions::Exception("Orderizer out of memory!");
          }
        }
        return this->vertex_count++;
      }

      void Orderizer::make_vert(int & index, double x, double y, double val)
      {
        index = add_vertex();
        verts[index][0] = x;
        verts[index][1] = y;
        verts[index][2] = val;
      }

      void Orderizer::reallocate_specific(int number_of_elements)
      {
        this->label_size = std::max(this->label_size, number_of_elements + 10);
        this->label_count = 0;

        if (this->verts)
          this->verts = (double3*)realloc(this->verts, sizeof(double3)* this->vertex_size);
        else
          this->verts = (double3*)malloc(sizeof(double3)* this->vertex_size);

        if (this->lvert)
          this->lvert = (int*)realloc(this->lvert, sizeof(int)* label_size);
        else
          this->lvert = (int*)malloc(sizeof(int)* label_size);

        if (this->ltext)
          ltext = (char**)realloc(this->ltext, sizeof(char*)* label_size);
        else
          ltext = (char**)malloc(sizeof(char*)* label_size);

        if (this->lbox)
          lbox = (double2*)realloc(this->lbox, sizeof(double2)* label_size);
        else
          lbox = (double2*)malloc(sizeof(double2)* label_size);

        if ((!this->lbox) || (!this->ltext) || (!this->verts) || (!this->lvert))
        {
          free();
          throw Exceptions::Exception("Orderizer out of memory!");
        }
      }

      template<typename Scalar>
      void Orderizer::process_space(SpaceSharedPtr<Scalar> space, bool show_edge_orders)
      {
        // sanity check
        if (space == nullptr)
          throw Hermes::Exceptions::Exception("Space is nullptr in Orderizer:process_space().");

        if (!space->is_up_to_date())
          throw Hermes::Exceptions::Exception("The space is not up to date.");

        MeshSharedPtr mesh = space->get_mesh();

        // Reallocate.
        this->reallocate_common(mesh);

        RefMap refmap;
        int type = 1;

        int oo, o[6];

        // make a mesh illustrating the distribution of polynomial orders over the space
        Element* e;
        for_all_active_elements(e, mesh)
        {
          oo = o[4] = o[5] = space->get_element_order(e->id);
          if (show_edge_orders)
          for (unsigned int k = 0; k < e->get_nvert(); k++)
            o[k] = space->get_edge_order(e, k);
          else if (e->is_curved())
          {
            if (e->is_triangle())
            for (unsigned int k = 0; k < e->get_nvert(); k++)
              o[k] = oo;
            else
            for (unsigned int k = 0; k < e->get_nvert(); k++)
              o[k] = H2D_GET_H_ORDER(oo);
          }

          double3* pt;
          int np;
          double* x;
          double* y;
          if (show_edge_orders || e->is_curved())
          {
            refmap.set_quad_2d(&quad_ord);
            refmap.set_active_element(e);
            x = refmap.get_phys_x(type);
            y = refmap.get_phys_y(type);

            pt = quad_ord.get_points(type, e->get_mode());
            np = quad_ord.get_num_points(type, e->get_mode());
          }
          else
          {
            refmap.set_quad_2d(&quad_ord_simple);
            refmap.set_active_element(e);
            x = refmap.get_phys_x(type);
            y = refmap.get_phys_y(type);

            pt = quad_ord_simple.get_points(type, e->get_mode());
            np = quad_ord_simple.get_num_points(type, e->get_mode());
          }

          int id[80];
          assert(np <= 80);

          int mode = e->get_mode();
          if (e->is_quad())
          {
            o[4] = H2D_GET_H_ORDER(oo);
            o[5] = H2D_GET_V_ORDER(oo);
          }
          if (show_edge_orders || e->is_curved())
          {
            make_vert(lvert[label_count], x[0], y[0], o[4]);

            for (int i = 1; i < np; i++)
              make_vert(id[i - 1], x[i], y[i], o[(int)pt[i][2]]);

            for (int i = 0; i < num_elem[mode][type]; i++)
              this->add_triangle(id[ord_elem[mode][type][i][0]], id[ord_elem[mode][type][i][1]], id[ord_elem[mode][type][i][2]], e->marker);

            for (int i = 0; i < num_edge[mode][type]; i++)
            {
              if (e->en[ord_edge[mode][type][i][2]]->bnd || (y[ord_edge[mode][type][i][0] + 1] < y[ord_edge[mode][type][i][1] + 1]) ||
                ((y[ord_edge[mode][type][i][0] + 1] == y[ord_edge[mode][type][i][1] + 1]) &&
                (x[ord_edge[mode][type][i][0] + 1] < x[ord_edge[mode][type][i][1] + 1])))
              {
                LinearizerBase::add_edge(id[ord_edge[mode][type][i][0]], id[ord_edge[mode][type][i][1]], e->en[ord_edge[mode][type][i][2]]->marker);
              }
            }
          }
          else
          {
            make_vert(lvert[label_count], x[0], y[0], o[4]);
            make_vert(lvert[label_count], x[0], y[0], o[5]);

            for (int i = 0; i < np; i++)
              make_vert(id[i], x[i], y[i], o[(int)pt[i][2]]);

            for (int i = 0; i < num_elem_simple[mode][type]; i++)
              this->add_triangle(id[ord_elem_simple[mode][type][i][0]], id[ord_elem_simple[mode][type][i][1]], id[ord_elem_simple[mode][type][i][2]], e->marker);

            for (int i = 0; i < num_edge_simple[mode][type]; i++)
            {
              LinearizerBase::add_edge(id[ord_edge_simple[mode][type][i][0]], id[ord_edge_simple[mode][type][i][1]], e->en[ord_edge_simple[mode][type][i][2]]->marker);
            }
          }

          double xmin = 1e100, ymin = 1e100, xmax = -1e100, ymax = -1e100;
          for (unsigned int k = 0; k < e->get_nvert(); k++)
          {
            if (e->vn[k]->x < xmin) xmin = e->vn[k]->x;
            if (e->vn[k]->x > xmax) xmax = e->vn[k]->x;
            if (e->vn[k]->y < ymin) ymin = e->vn[k]->y;
            if (e->vn[k]->y > ymax) ymax = e->vn[k]->y;
          }
          lbox[label_count][0] = xmax - xmin;
          lbox[label_count][1] = ymax - ymin;
          ltext[label_count++] = labels[o[4]][o[5]];
        }

        refmap.set_quad_2d(&g_quad_2d_std);
      }
      
      void Orderizer::free()
      {
        if (verts != nullptr)
        {
          ::free(verts);
          verts = nullptr;
        }
        if (lvert != nullptr)
        {
          ::free(lvert);
          lvert = nullptr;
        }
        if (ltext != nullptr)
        {
          ::free(ltext);
          ltext = nullptr;
        }
        if (lbox != nullptr)
        {
          ::free(lbox);
          lbox = nullptr;
        }

        LinearizerBase::free();
      }

      Orderizer::~Orderizer()
      {
        free();
      }

      template<typename Scalar>
      void Orderizer::save_orders_vtk(SpaceSharedPtr<Scalar> space, const char* file_name)
      {
        process_space(space);

        FILE* f = fopen(file_name, "wb");
        if (f == nullptr) throw Hermes::Exceptions::Exception("Could not open %s for writing.", file_name);

        // Output header for vertices.
        fprintf(f, "# vtk DataFile Version 2.0\n");
        fprintf(f, "\n");
        fprintf(f, "ASCII\n\n");
        fprintf(f, "DATASET UNSTRUCTURED_GRID\n");

        // Output vertices.
        fprintf(f, "POINTS %d %s\n", this->vertex_count, "float");
        for (int i = 0; i < this->vertex_count; i++)
        {
          fprintf(f, "%g %g %g\n", this->verts[i][0], this->verts[i][1], 0.);
        }

        // Output elements.
        fprintf(f, "\n");
        fprintf(f, "CELLS %d %d\n", this->triangle_count, 4 * this->triangle_count);
        for (int i = 0; i < this->triangle_count; i++)
        {
          fprintf(f, "3 %d %d %d\n", this->tris[i][0], this->tris[i][1], this->tris[i][2]);
        }

        // Output cell types.
        fprintf(f, "\n");
        fprintf(f, "CELL_TYPES %d\n", this->triangle_count);

        for (int i = 0; i < this->triangle_count; i++)
          fprintf(f, "5\n");    // The "5" means triangle in VTK.

        // This outputs double solution values. Look into Hermes2D/src/output/vtk.cpp
        // for how it is done for vectors.
        fprintf(f, "\n");
        fprintf(f, "POINT_DATA %d\n", this->vertex_count);
        fprintf(f, "SCALARS %s %s %d\n", "Mesh", "float", 1);
        fprintf(f, "LOOKUP_TABLE %s\n", "default");
        for (int i = 0; i < this->vertex_count; i++)
          fprintf(f, "%g \n", this->verts[i][2]);
        fclose(f);
      }

      template<typename Scalar>
      void Orderizer::save_markers_vtk(SpaceSharedPtr<Scalar> space, const char* file_name)
      {
        process_space(space);

        FILE* f = fopen(file_name, "wb");
        if (f == nullptr) throw Hermes::Exceptions::Exception("Could not open %s for writing.", file_name);

        // Output header for vertices.
        fprintf(f, "# vtk DataFile Version 2.0\n");
        fprintf(f, "\n");
        fprintf(f, "ASCII\n\n");
        fprintf(f, "DATASET UNSTRUCTURED_GRID\n");

        // Output vertices.
        fprintf(f, "POINTS %d %s\n", this->vertex_count, "float");
        for (int i = 0; i < this->vertex_count; i++)
        {
          fprintf(f, "%g %g %g\n", this->verts[i][0], this->verts[i][1], 0.);
        }

        // Output elements.
        fprintf(f, "\n");
        fprintf(f, "CELLS %d %d\n", this->triangle_count, 4 * this->triangle_count);
        for (int i = 0; i < this->triangle_count; i++)
        {
          fprintf(f, "3 %d %d %d\n", this->tris[i][0], this->tris[i][1], this->tris[i][2]);
        }

        // Output cell types.
        fprintf(f, "\n");
        fprintf(f, "CELL_TYPES %d\n", this->triangle_count);

        for (int i = 0; i < this->triangle_count; i++)
          fprintf(f, "5\n");    // The "5" means triangle in VTK.

        // This outputs double solution values. Look into Hermes2D/src/output/vtk.cpp
        // for how it is done for vectors.
        fprintf(f, "\n");
        fprintf(f, "CELL_DATA %d\n", this->triangle_count);
        fprintf(f, "SCALARS %s %s %d\n", "Mesh", "float", 1);
        fprintf(f, "LOOKUP_TABLE %s\n", "default");
        for (int i = 0; i < this->triangle_count; i++)
          fprintf(f, "%d \n", this->tri_markers[i]);
        fclose(f);
      }

      template<typename Scalar>
      void Orderizer::save_mesh_vtk(SpaceSharedPtr<Scalar> space, const char* file_name)
      {
        process_space(space);

        FILE* f = fopen(file_name, "wb");
        if (f == nullptr) throw Hermes::Exceptions::Exception("Could not open %s for writing.", file_name);

        // Output header for vertices.
        fprintf(f, "# vtk DataFile Version 2.0\n");
        fprintf(f, "\n");
        fprintf(f, "ASCII\n\n");
        fprintf(f, "DATASET UNSTRUCTURED_GRID\n");

        // Output vertices.
        fprintf(f, "POINTS %d %s\n", this->vertex_count, "float");
        for (int i = 0; i < this->vertex_count; i++)
          fprintf(f, "%g %g %g\n", this->verts[i][0], this->verts[i][1], 0.0);

        // Output elements.
        fprintf(f, "\n");
        fprintf(f, "CELLS %d %d\n", this->edges_count, +3 * this->edges_count);
        for (int i = 0; i < this->edges_count; i++)
          fprintf(f, "2 %d %d\n", this->edges[i][0], this->edges[i][1]);

        // Output cell types.
        fprintf(f, "\n");
        fprintf(f, "CELL_TYPES %d\n", this->edges_count);

        for (int i = 0; i < this->edges_count; i++)
          fprintf(f, "3\n");    // The "3" means line in VTK.

        // This outputs double solution values. Look into Hermes2D/src/output/vtk.cpp
        // for how it is done for vectors.
        fprintf(f, "\n");
        fprintf(f, "CELL_DATA %d\n", this->edges_count);
        fprintf(f, "SCALARS %s %s %d\n", "Mesh", "float", 1);
        fprintf(f, "LOOKUP_TABLE %s\n", "default");
        for (int i = 0; i < this->edges_count; i++)
          fprintf(f, "0 \n");
        fclose(f);
      }

      int Orderizer::get_labels(int*& lvert, char**& ltext, double2*& lbox) const
      {
        lvert = this->lvert;
        ltext = this->ltext;
        lbox = this->lbox;
        return label_count;
      }

      void Orderizer::calc_vertices_aabb(double* min_x, double* max_x, double* min_y, double* max_y) const
      {
        if (verts == nullptr)
          throw Exceptions::Exception("Cannot calculate AABB from nullptr vertices");
        calc_aabb(&verts[0][0], &verts[0][1], sizeof(double3), vertex_count, min_x, max_x, min_y, max_y);
      }

      double3* Orderizer::get_vertices()
      {
        return this->verts;
      }
      int Orderizer::get_num_vertices()
      {
        return this->vertex_count;
      }

      template HERMES_API void Orderizer::save_orders_vtk<double>(const SpaceSharedPtr<double> space, const char* file_name);
      template HERMES_API void Orderizer::save_orders_vtk<std::complex<double> >(const SpaceSharedPtr<std::complex<double> > space, const char* file_name);
      template HERMES_API void Orderizer::save_markers_vtk<double>(const SpaceSharedPtr<double> space, const char* file_name);
      template HERMES_API void Orderizer::save_markers_vtk<std::complex<double> >(const SpaceSharedPtr<std::complex<double> > space, const char* file_name);
      template HERMES_API void Orderizer::save_mesh_vtk<double>(const SpaceSharedPtr<double> space, const char* file_name);
      template HERMES_API void Orderizer::save_mesh_vtk<std::complex<double> >(const SpaceSharedPtr<std::complex<double> > space, const char* file_name);
      template HERMES_API void Orderizer::process_space<double>(const SpaceSharedPtr<double> space, bool);
      template HERMES_API void Orderizer::process_space<std::complex<double> >(const SpaceSharedPtr<std::complex<double> > space, bool);
    }
  }
}