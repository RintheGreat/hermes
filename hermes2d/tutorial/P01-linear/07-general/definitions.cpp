#include "weakform/weakform.h"
#include "integrals/integrals_h1.h"
#include "boundaryconditions/essential_bcs.h"

class EssentialBCNonConst : public EssentialBC {
public:
  EssentialBCNonConst(std::string marker) 
           : EssentialBC(Hermes::vector<std::string>())
  {
    markers.push_back(marker);
  }

  ~EssentialBCNonConst() {};

  inline EssentialBCValueType get_value_type() const 
         { return EssentialBC::BC_FUNCTION; }

  scalar function(double x, double y) const
  {
    return -cos(M_PI*x);
  }
};

class CustomWeakFormGeneral : public WeakForm
{
public:
  CustomWeakFormGeneral() : WeakForm(1)
  {
    add_matrix_form(new MatrixFormVolGeneral(0, 0));
    add_vector_form(new VectorFormVolGeneral(0));
    add_vector_form_surf(new VectorFormSurfGeneral(0, BDY_VERTICAL));
  }

  ~CustomWeakFormGeneral() {}

private:

  // Problem parameters.
  double a_11(double x, double y) { if (y > 0) return 1 + x*x + y*y; else return 1;}
  double a_22(double x, double y) { if (y > 0) return 1; else return 1 + x*x + y*y;}
  double a_12(double x, double y) { return 1; }
  double a_21(double x, double y) { return 1;}
  double a_1(double x, double y) { return 0.0;}
  double a_2(double x, double y) { return 0.0;}
  double a_0(double x, double y) { return 0.0;}
  double rhs(double x, double y) { return 1 + x*x + y*y;}
  double g_N(double x, double y) { return 0;}

  class MatrixFormVolGeneral : public WeakForm::MatrixFormVol
  {
  public:
    MatrixFormVolGeneral(int i, int j) : WeakForm::MatrixFormVol(i, j, HERMES_SYM) { }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *u, 
                 Func<double> *v, Geom<double> *e, ExtData<scalar> *ext)
    {
      scalar result = 0;
      for (int i=0; i < n; i++) {
        double x = e->x[i];
        double y = e->y[i];
        result += (static_cast<CustomWeakFormGeneral *>(wf)->a_11(x, y)*u->dx[i]*v->dx[i] +
                   static_cast<CustomWeakFormGeneral *>(wf)->a_12(x, y)*u->dy[i]*v->dx[i] +
                   static_cast<CustomWeakFormGeneral *>(wf)->a_21(x, y)*u->dx[i]*v->dy[i] +
                   static_cast<CustomWeakFormGeneral *>(wf)->a_22(x, y)*u->dy[i]*v->dy[i] +
                   static_cast<CustomWeakFormGeneral *>(wf)->a_1(x, y)*u->dx[i]*v->val[i] +
                   static_cast<CustomWeakFormGeneral *>(wf)->a_2(x, y)*u->dy[i]*v->val[i] +
                   static_cast<CustomWeakFormGeneral *>(wf)->a_0(x, y)*u->val[i]*v->val[i]) * wt[i];
      }
      return result;
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v, 
            Geom<Ord> *e, ExtData<Ord> *ext)
    {
      return u->val[0] * v->val[0] * e->x[0] * e->x[0]; // returning the sum of the degrees of the basis
      // and test function plus two
    }
  };

  class VectorFormVolGeneral : public WeakForm::VectorFormVol
  {
  public:
    VectorFormVolGeneral(int i) : WeakForm::VectorFormVol(i) {}

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, Geom<double> *e, ExtData<scalar> *ext)
    {
      scalar result = 0;
      for (int i = 0; i < n; i++)
        result += wt[i] * (static_cast<CustomWeakFormGeneral *>(wf)->rhs(e->x[i], e->y[i]) * v->val[i]);
      return result;
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, ExtData<Ord> *ext)
    {
      return v->val[0] * e->x[0] * e->x[0];  // returning the polynomial degree of the test function plus two
    }
  };

  class VectorFormSurfGeneral : public WeakForm::VectorFormSurf
  {
  public:
    VectorFormSurfGeneral(int i, std::string area = HERMES_ANY) : WeakForm::VectorFormSurf(i, area)
    {
      adapt_eval = false;
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, Geom<double> *e, ExtData<scalar> *ext)
    {
      scalar result = 0;
      for (int i = 0; i < n; i++)
        result += wt[i] * (static_cast<CustomWeakFormGeneral *>(wf)->g_N(e->x[i], e->y[i]) * v->val[i]);
      return result;
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, ExtData<Ord> *ext)
    {
      return v->val[0] * e->x[0] * e->x[0];  // returning the polynomial degree of the test function plus two
    }
  };
};