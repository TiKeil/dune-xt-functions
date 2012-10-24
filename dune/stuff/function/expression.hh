#ifndef DUNE_STUFF_FUNCTION_EXPRESSION_HH
#define DUNE_STUFF_FUNCTION_EXPRESSION_HH

#ifdef HAVE_CMAKE_CONFIG
#include "cmake_config.h"
#else
#include "config.h"
#endif // ifdef HAVE_CMAKE_CONFIG

// system
#include <sstream>

// dune-common
#include <dune/common/fvector.hh>
#include <dune/common/dynvector.hh>
#include <dune/common/parametertree.hh>
#include <dune/common/exceptions.hh>

// eigen
#ifdef HAVE_EIGEN
#include <Eigen/Core>
#endif // HAVE_EIGEN

#ifdef HAVE_DUNE_FEM
#include <dune/fem/function/common/function.hh>
#include <dune/fem/space/common/functionspace.hh>
#endif

// local
#include "expression/mathexpr.hh"
#include "interface.hh"

namespace Dune {

namespace Stuff {

namespace Function {

/**
  \brief  Provides a function which evaluates a given mathematical expression at runtime.

          Given a mathematical expression as a string, a domain \f$ K_d^{m \geq 1} \f$ and a range \f$ K_r^{n \geq 1}
          \f$ this function represents the map
          \f{eqnarray}
            f:K_d^m \to K_r^n\\
            x = (x_1, \dots, x_m)' \mapsto (f_1(x), \dots f_n(x))',
          \f}
          where \f$ K_d \f$ is the DomainType and \f$ K_r \f$ is the RangeType, usually a power of \f$ \mathcal{R} \f$.
          The name of the variable as well as the \f$ n \f$ expressions of \f$f_1, \dots, f_n\f$ have to be given in a
          Dune::ParameterTree in the following form:
\code variable: x
expression.0: 2*x[0]
expression.1: sin(x[1])*x[0]\endcode
          There have to exist at least \f$n\f$ expressions; the entries of the variable are indexed by \f$[i]\f$ for
          \f$ 0 \leq i \leq m - 1 \f$.
 **/
template <class DomainFieldImp, int maxDimDomain, class RangeFieldImp, int maxDimRange>
class Expression : public Interface<DomainFieldImp, maxDimDomain, RangeFieldImp, maxDimRange>
{
public:
  typedef DomainFieldImp DomainFieldType;

  typedef RangeFieldImp RangeFieldType;

  Expression(const Dune::ParameterTree& paramTree)
  {
    // assert dims
    assert(maxDimDomain > 0);
    assert(maxDimRange > 0);
    // get variable
    if (paramTree.hasKey("variable")) {
      variable_ = paramTree.get("variable", "");
    } else {
      std::stringstream msg;
      msg << "Error in " << id() << ": key 'variable' not found in the following Dune::Parametertree" << std::endl;
      paramTree.report(msg);
      DUNE_THROW(Dune::InvalidStateException, msg.str());
    }
    for (unsigned int i = 0; i < maxDimDomain; ++i) {
      std::stringstream tmp_variable;
      tmp_variable << variable_ << "[" << i << "]";
      variables_.push_back(tmp_variable.str());
    }
    // get expressions
    bool goOn      = true;
    unsigned int i = 0;
    while (goOn) {
      if (i < maxDimRange) {
        std::stringstream tmp_expression;
        tmp_expression << "expression." << i;
        if (paramTree.hasKey(tmp_expression.str())) {
          expressions_.push_back(paramTree.get(tmp_expression.str(), ""));
          ++i;
        } else {
          goOn = false;
        }
      } else {
        goOn = false;
      } // if (i < maxDimRange) {
      actualDimRange_ = i;
    } // while (goOn)
    if (actualDimRange_ < 1) {
      std::stringstream msg;
      msg << "Error in " << id()
          << ": no subtree 'expression' with keys 0, 1, ... found in the following Dune::Parametertree" << std::endl;
      paramTree.report(msg);
      DUNE_THROW(Dune::InvalidStateException, msg.str());
    }
    // set up
    setUp();
  } // Expression(Dune::ParameterTree& paramTree)


  Expression(const std::string variable_in, const std::vector<std::string>& expressions_in)
    : variable_(variable_in)
    , expressions_(expressions_in)
    , actualDimRange_(expressions_.size())
  {
    // assert dims
    assert(maxDimDomain > 0);
    assert(maxDimRange > 0);

    for (int i = 0; i < maxDimDomain; ++i) {
      std::stringstream variableStream;
      variableStream << variable_ << "[" << i << "]";
      variables_.push_back(variableStream.str());
    }
    if (actualDimRange_ < 1) {
      std::stringstream msg;
      msg << "Error in " << id << ": Given expressions-vector is empty!" << std::endl;
      DUNE_THROW(Dune::InvalidStateException, msg.str());
    }
    // set up
    setUp();
  } // Expression(Dune::ParameterTree& paramTree)


  ~Expression()
  {
    for (unsigned int i = 0; i < dimRange(); ++i) {
      delete op_[i];
    }
    for (unsigned int i = 0; i < maxDimDomain; ++i) {
      delete var_arg_[i];
      delete arg_[i];
    }
  } // ~Expression()

  static const std::string id()
  {
    return "stuff.function.expression";
  }

  std::string variable() const
  {
    return variable_;
  }

  std::string expression(unsigned int i) const
  {
    assert(i < dimRange());
    return expressions_[i];
  } // std::string expression(int i) const

  const std::vector<std::string>& expressions() const
  {
    return expressions_;
  }

  unsigned int dimRange() const
  {
    return actualDimRange_;
  }

  //! needed for Interface
  virtual void evaluate(const Dune::FieldVector<DomainFieldImp, maxDimDomain>& arg,
                        Dune::FieldVector<RangeFieldImp, maxDimRange>& ret) const
  {
    // ensure right dimensions
    assert(arg.size() <= maxDimDomain);
    assert(ret.size() <= dimRange());
    // arg
    for (typename Dune::FieldVector<DomainFieldImp, maxDimDomain>::size_type i = 0; i < arg.size(); ++i) {
      *(arg_[i]) = arg[i];
    }
    // ret
    for (typename Dune::FieldVector<RangeFieldImp, maxDimRange>::size_type i = 0; i < ret.size(); ++i) {
      ret[i] = op_[i]->Val();
    }
  }

  template <class DomainVectorType, class RangeVectorType>
  void evaluate(const Dune::DenseVector<DomainVectorType>& arg, Dune::DenseVector<RangeVectorType>& ret) const
  {
    // ensure right dimensions
    assert(arg.size() <= maxDimDomain);
    assert(ret.size() <= dimRange());
    // arg
    for (typename Dune::DenseVector<DomainVectorType>::size_type i = 0; i < arg.size(); ++i) {
      *(arg_[i]) = arg[i];
    }
    // ret
    for (typename Dune::DenseVector<RangeVectorType>::size_type i = 0; i < ret.size(); ++i) {
      ret[i] = op_[i]->Val();
    }
  }

#ifdef HAVE_EIGEN
  void evaluate(const Eigen::VectorXd& arg, Eigen::VectorXd& ret) const
  {
    // ensure right dimensions
    assert(arg.rows() <= maxDimDomain);
    assert(ret.rows() <= dimRange());
    // arg
    for (int i = 0; i < arg.rows(); ++i) {
      *(arg_[i]) = arg(i);
    }
    // ret
    for (int i = 0; i < ret.size(); ++i) {
      ret(i) = op_[i]->Val();
    }
  }
#endif // HAVE_EIGEN

private:
  void setUp()
  {
    for (unsigned int i = 0; i < maxDimDomain; ++i) {
      arg_[i]      = new DomainFieldType(0.0);
      var_arg_[i]  = new RVar(variables_[i].c_str(), arg_[i]);
      vararray_[i] = var_arg_[i];
    }
    for (unsigned int i = 0; i < dimRange(); ++i) {
      op_[i] = new ROperation(expressions_[i].c_str(), maxDimDomain, vararray_);
    }
  } // void setUp()

  std::string variable_;
  std::vector<std::string> variables_;
  std::vector<std::string> expressions_;
  unsigned int actualDimRange_;
  mutable DomainFieldType* arg_[maxDimDomain];
  RVar* var_arg_[maxDimDomain];
  RVar* vararray_[maxDimDomain];
  ROperation* op_[maxDimRange];
}; // class Expression

} // namespace Function
} // namespace Stuff
} // namespace Dune

#endif // DUNE_STUFF_FUNCTION_EXPRESSION_HH
