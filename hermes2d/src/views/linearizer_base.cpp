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

#include "linearizer_base.h"
#include "exact_solution.h"

namespace Hermes
{
  namespace Hermes2D
  {
    namespace Views
    {
      Quad2DLin g_quad_lin;

      Quad2DLin::Quad2DLin()
      {
        max_order[0] = max_order[1] = 1;
        num_tables[0] = num_tables[1] = 2;
        tables = lin_tables;
        np = lin_np;
      };

      double LinearizerBase::large_elements_fraction_of_mesh_size_threshold = 1e-2;

      LinearizerBase::LinearizerBase(bool auto_max) : auto_max(auto_max), empty(true), states(nullptr), num_states(0)
      {
        tris = nullptr;
        tri_markers = nullptr;
        edges = nullptr;
        edge_markers = nullptr;
        hash_table = nullptr;
        info = nullptr;
        max = -1e100;

        vertex_count = triangle_count = edges_count = this->vertex_size = this->triangle_size = this->edges_size = 0;

#ifndef NOGLUT
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&data_mutex, &attr);
        pthread_mutexattr_destroy(&attr);
#endif
        this->level_map = nullptr;
      }

      bool LinearizerBase::is_empty()
      {
        return this->empty;
      }

      void LinearizerBase::free()
      {
        if (tris != nullptr)
        {
          ::free(tris);
          tris = nullptr;
          ::free(tri_markers);
          tri_markers = nullptr;
        }
        if (edges != nullptr)
        {
          ::free(edges);
          edges = nullptr;
          ::free(edge_markers);
          edge_markers = nullptr;
        }
        if (this->level_map)
        {
          delete[] level_map;
          level_map = nullptr;
        }
        this->empty = true;
      }

      LinearizerBase::~LinearizerBase()
      {
#ifndef NOGLUT
        pthread_mutex_destroy(&data_mutex);
#endif
      }

      void LinearizerBase::lock_data() const
      {
#ifndef NOGLUT
        pthread_mutex_lock(&data_mutex);
#endif
      }

      void LinearizerBase::unlock_data() const
      {
#ifndef NOGLUT
        pthread_mutex_unlock(&data_mutex);
#endif
      }

      void LinearizerBase::process_edge(int iv1, int iv2, int marker)
      {
        int mid = peek_vertex(iv1, iv2);
        if (mid != -1)
        {
          process_edge(iv1, mid, marker);
          process_edge(mid, iv2, marker);
        }
        else
          add_edge(iv1, iv2, marker);
      }

      void LinearizerBase::init_linearizer_base(MeshFunctionSharedPtr<double> sln)
      {
        lock_data();
        if (this->level_map)
        {
          delete[] level_map;
          level_map = nullptr;
        }
        this->level_map = new int[sln->get_mesh()->get_max_element_id()];
        memset(this->level_map, -1, sizeof(int)* sln->get_mesh()->get_max_element_id());
      }

      void LinearizerBase::deinit_linearizer_base()
      {
        unlock_data();
      }

      int LinearizerBase::get_max_level(Element* e, int polynomial_order, MeshSharedPtr mesh)
      {
        if (this->level_map[e->id] != -1)
          return this->level_map[e->id];

        if (e->is_curved())
          this->level_map[e->id] = LIN_MAX_LEVEL;
        else
        {
          double element_area = e->get_area();
          double bottom_left_x, bottom_left_y, top_right_x, top_right_y;
          mesh->get_bounding_box(bottom_left_x, bottom_left_y, top_right_x, top_right_y);
          double mesh_size_x = top_right_x - bottom_left_x, mesh_size_y = top_right_y - bottom_left_y;

          double mesh_size_times_threshold = LinearizerBase::large_elements_fraction_of_mesh_size_threshold * mesh_size_x * mesh_size_y;
          double ratio = (LIN_MAX_LEVEL * std::pow(element_area / mesh_size_times_threshold, 0.2)) * (std::sqrt((double)polynomial_order - 1));
          this->level_map[e->id] = std::min<int>(LIN_MAX_LEVEL, (int)ratio);
        }

        if (e->is_quad() && polynomial_order == 1)
          this->level_map[e->id] = 2;
        return this->level_map[e->id];
      }

      void LinearizerBase::regularize_triangle(int iv0, int iv1, int iv2, int mid0, int mid1, int mid2, int marker)
      {
        // count the number of hanging mid-edge vertices
        int n = 0;
        if (mid0 >= 0) n++;
        if (mid1 >= 0) n++;
        if (mid2 >= 0) n++;
        if (n == 3)
        {
          // three hanging vertices: split into four triangles
          regularize_triangle(iv0, mid0, mid2, peek_vertex(iv0, mid0), -1, peek_vertex(mid2, iv0), marker);
          regularize_triangle(mid0, iv1, mid1, peek_vertex(mid0, iv1), peek_vertex(iv1, mid1), -1, marker);
          regularize_triangle(mid2, mid1, iv2, -1, peek_vertex(mid1, iv2), peek_vertex(iv2, mid2), marker);
          regularize_triangle(mid0, mid1, mid2, -1, -1, -1, marker);
        }
        else if (n == 2)
        {
          // two hanging vertices: split into three triangles
          if (mid0 < 0)
          {
            regularize_triangle(iv0, iv1, mid1, peek_vertex(iv0, iv1), peek_vertex(iv1, mid1), -1, marker);
            regularize_triangle(mid2, iv0, mid1, peek_vertex(mid2, iv0), -1, -1, marker);
            regularize_triangle(mid2, mid1, iv2, -1, peek_vertex(mid1, iv2), peek_vertex(iv2, mid2), marker);
          }
          else if (mid1 < 0)
          {
            regularize_triangle(iv1, iv2, mid2, peek_vertex(iv1, iv2), peek_vertex(iv2, mid2), -1, marker);
            regularize_triangle(mid0, iv1, mid2, peek_vertex(mid0, iv1), -1, -1, marker);
            regularize_triangle(mid0, mid2, iv0, -1, peek_vertex(mid2, iv0), peek_vertex(iv0, mid0), marker);
          }
          else
          {
            regularize_triangle(iv2, iv0, mid0, peek_vertex(iv2, iv0), peek_vertex(iv0, mid0), -1, marker);
            regularize_triangle(mid1, iv2, mid0, peek_vertex(mid1, iv2), -1, -1, marker);
            regularize_triangle(mid1, mid0, iv1, -1, peek_vertex(mid0, iv1), peek_vertex(iv1, mid1), marker);
          }
        }
        else if (n == 1)
        {
          // one hanging vertex: split into two triangles
          if (mid0 >= 0)
          {
            regularize_triangle(iv0, mid0, iv2, peek_vertex(iv0, mid0), -1, peek_vertex(iv2, iv0), marker);
            regularize_triangle(mid0, iv1, iv2, peek_vertex(mid0, iv1), peek_vertex(iv1, iv2), -1, marker);
          }
          else if (mid1 >= 0)
          {
            regularize_triangle(iv1, mid1, iv0, peek_vertex(iv1, mid1), -1, peek_vertex(iv0, iv1), marker);
            regularize_triangle(mid1, iv2, iv0, peek_vertex(mid1, iv2), peek_vertex(iv2, iv0), -1, marker);
          }
          else
          {
            regularize_triangle(iv2, mid2, iv1, peek_vertex(iv2, mid2), -1, peek_vertex(iv1, iv2), marker);
            regularize_triangle(mid2, iv0, iv1, peek_vertex(mid2, iv0), peek_vertex(iv0, iv1), -1, marker);
          }
        }
        else
        {
          // no hanging vertices: produce a single triangle
          add_triangle(iv0, iv1, iv2, marker);
        }
      }

      void LinearizerBase::add_edge(int iv1, int iv2, int marker)
      {
#pragma omp critical(realloc_edges)
        {
          if (edges_count >= edges_size)
          {
            void* new_edges = realloc(edges, sizeof(int2)* this->edges_size * 1.5);
            if (new_edges)
              edges = (int2*)new_edges;
            else
            {
              this->free();
              this->deinit_linearizer_base();
              throw Exceptions::Exception("A linearizer out of memory!");
            }

            void* new_edge_markers = realloc(edge_markers, sizeof(int)*  (this->edges_size = this->edges_size * 1.5));
            if (new_edge_markers)
              edge_markers = (int*)new_edge_markers;
            else
            {
              this->free();
              this->deinit_linearizer_base();
              throw Exceptions::Exception("A linearizer out of memory!");
            }
          }
          edges[edges_count][0] = iv1;
          edges[edges_count][1] = iv2;
          edge_markers[edges_count++] = marker;
        }
      }

      int LinearizerBase::peek_vertex(int p1, int p2)
      {
        // search for a vertex with parents p1, p2
        if (p1 > p2) std::swap(p1, p2);
        int index = hash(p1, p2);
        int i = hash_table[index];
        while (i >= 0)
        {
          if (info[i][0] == p1 && info[i][1] == p2) return i;
          i = info[i][2];
        }
        return -1;
      }

      void LinearizerBase::add_triangle(int iv0, int iv1, int iv2, int marker)
      {
        int index;
#pragma omp critical(realloc_triangles)
        {
          if (triangle_count >= triangle_size)
          {
            void* new_tris = realloc(tris, sizeof(int3)* triangle_size * 1.5);
            if (new_tris)
              tris = (int3*)new_tris;
            else
            {
              this->free();
              this->deinit_linearizer_base();
              throw Exceptions::Exception("A linearizer out of memory!");
            }

            void* new_tri_markers = realloc(tri_markers, sizeof(int)* (triangle_size = triangle_size * 1.5));
            if (new_tri_markers)
              tri_markers = (int*)new_tri_markers;
            else
            {
              this->free();
              this->deinit_linearizer_base();
              throw Exceptions::Exception("A linearizer out of memory!");
            }
          }
          index = triangle_count++;
        }

        tris[index][0] = iv0;
        tris[index][1] = iv1;
        tris[index][2] = iv2;
        tri_markers[index] = marker;
      }

      int LinearizerBase::hash(int p1, int p2)
      {
        return (984120265 * p1 + 125965121 * p2) & (vertex_size - 1);
      }

      void LinearizerBase::set_max_absolute_value(double max_abs)
      {
        if (max_abs < 0.0)
          this->warn("Setting of maximum absolute value in Linearizer with a negative value");
        else
        {
          this->auto_max = false;
          this->max = max_abs;
        }
        return;
      }

      double LinearizerBase::get_min_value() const
      {
        return min_val;
      }

      double LinearizerBase::get_max_value() const
      {
        return max_val;
      }

      void LinearizerBase::calc_aabb(double* x, double* y, int stride, int num, double* min_x, double* max_x, double* min_y, double* max_y)
      {
        *min_x = *max_x = *x;
        *min_y = *max_y = *y;

        uint8_t* ptr_x = (uint8_t*)x;
        uint8_t* ptr_y = (uint8_t*)y;
        for (int i = 0; i < num; i++, ptr_x += stride, ptr_y += stride)
        {
          *min_x = std::min(*min_x, *((double*)ptr_x));
          *min_y = std::min(*min_y, *((double*)ptr_y));
          *max_x = std::max(*max_x, *((double*)ptr_x));
          *max_y = std::max(*max_y, *((double*)ptr_y));
        }
      }

      int3* LinearizerBase::get_triangles()
      {
        return this->tris;
      }
      int* LinearizerBase::get_triangle_markers()
      {
        return this->tri_markers;
      }
      int LinearizerBase::get_num_triangles()
      {
        return this->triangle_count;
      }
      int2* LinearizerBase::get_edges()
      {
        return this->edges;
      }
      int* LinearizerBase::get_edge_markers()
      {
        return this->edge_markers;
      }
      int LinearizerBase::get_num_edges()
      {
        return this->edges_count;
      }

      static const int default_allocation_multiplier_vertices = 6;
      static const int default_allocation_multiplier_triangles = 6;
      static const int default_allocation_multiplier_edges = 10;

      static const int default_allocation_minsize_vertices = 10000;
      static const int default_allocation_minsize_triangles = 10000;
      static const int default_allocation_minsize_edges = 15000;

      void LinearizerBase::reallocate_common(MeshSharedPtr mesh)
      {
        int number_of_elements = mesh->get_num_elements();

        this->vertex_size = std::max(default_allocation_multiplier_vertices * number_of_elements, std::max(this->vertex_size, default_allocation_minsize_vertices));
        this->triangle_size = std::max(default_allocation_multiplier_triangles * number_of_elements, std::max(this->triangle_size, default_allocation_minsize_triangles));
        this->edges_size = std::max(default_allocation_multiplier_edges * number_of_elements, std::max(this->edges_size, default_allocation_minsize_edges));

        // Set count.
        this->vertex_count = 0;
        this->triangle_count = 0;
        this->edges_count = 0;

        void* new_tris = realloc(tris, sizeof(int3)* this->triangle_size);
        if (new_tris)
          tris = (int3*)new_tris;
        else
        {
          this->free();
          this->deinit_linearizer_base();
          throw Exceptions::Exception("A linearizer out of memory!");
        }

        void* new_tri_markers = realloc(tri_markers, sizeof(int)* this->triangle_size);
        if (new_tri_markers)
          tri_markers = (int*)new_tri_markers;
        else
        {
          this->deinit_linearizer_base();
          throw Exceptions::Exception("A linearizer out of memory!");
        }

        void* new_edges = realloc(edges, sizeof(int2)* this->edges_size);
        if (new_edges)
          edges = (int2*)new_edges;
        else
        {
          this->free();
          this->deinit_linearizer_base();
          throw Exceptions::Exception("A linearizer out of memory!");
        }

        void* new_edge_markers = realloc(edge_markers, sizeof(int)* this->edges_size);
        if (new_edge_markers)
          edge_markers = (int*)new_edge_markers;
        else
        {
          this->free();
          this->deinit_linearizer_base();
          throw Exceptions::Exception("A linearizer out of memory!");
        }

        this->empty = false;

        this->reallocate_specific(number_of_elements);
      }
    }
  }
}
