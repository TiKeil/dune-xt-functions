// This file is part of the dune-xt-functions project:
//   https://github.com/dune-community/dune-xt-functions
// Copyright 2009-2017 dune-xt-functions developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2017)
//   Tobias Leibner  (2017)

#ifndef DUNE_XT_FUNCTIONS_INTERFACES_LOCAL_FUNCTIONS_HH
#define DUNE_XT_FUNCTIONS_INTERFACES_LOCAL_FUNCTIONS_HH

#include <array>
#include <sstream>
#include <vector>
#include <type_traits>

#include <dune/common/fvector.hh>

#include <dune/geometry/referenceelements.hh>

#include <dune/xt/common/fmatrix.hh>
#include <dune/xt/common/fvector.hh>
#include <dune/xt/common/memory.hh>
#include <dune/xt/common/parameter.hh>
#include <dune/xt/grid/entity.hh>
#include <dune/xt/grid/type_traits.hh>

#include <dune/xt/functions/exceptions.hh>

namespace Dune {
namespace XT {
namespace Functions {
namespace internal {


template <class R, size_t r, size_t rC>
struct RangeTypeSelector
{
  using type = XT::Common::FieldMatrix<R, r, rC>;
};

template <class R, size_t r>
struct RangeTypeSelector<R, r, 1>
{
  using type = XT::Common::FieldVector<R, r>;
};


template <size_t d, class R, size_t r, size_t rC>
struct DerivativeRangeTypeSelector
{
  using single_type = XT::Common::FieldVector<R, d>;
  using type = XT::Common::FieldVector<Dune::XT::Common::FieldMatrix<R, rC, d>, r>;
};

template <size_t d, class R, size_t r>
struct DerivativeRangeTypeSelector<d, R, r, 1>
{
  using single_type = XT::Common::FieldVector<R, d>;
  using type = XT::Common::FieldMatrix<R, r, d>;
};


} // namespace internal


/**
 *  \brief Interface for a set of globalvalued functions, which can be evaluated locally on one Entity.
 *
 *         Given a function u: R^d -> R^{r x rC}, we interpret it as a r functions u_1, ..., u_r in the sense of
 *         u(x) = (u_1(x), ..., u_r(x))T, where each u_s: R^d -> R^rC for 1 \leq s \leq r, and thus ultimately
 *
 *                | (u_1(x))_1, (u_1(x))_2, ..., (u_1(x))_rC |
 *                | (u_2(x))_1,    .        ...      .       |
 *         u(x) = |      .         .        ...      .       |
 *                |      .         .        ...      .       |
 *                | (u_r(x))_1, (u_r(x))_2, ..., (u_r(x))_rC |
 *
 *         Here, (u_s(x))_i can be interpreted as the evaluation of a scalar function u_s_i: R^d -> R
 *         for 1 \leq i \leq rC, and we can thus identify
 *
 *         u = ( u_s_i )_{s = 1, ..., r | i = 1, ..., rC }
 *
 *         This interpretation of u is used in RangeTypeSelector, with a special case if rC = 1, to simplify manners.
 *
 *         Regarding derivatives: since one is often interested in the derivatives of the individual functions u_s, it
 *         makes sense to iterate over r first, and we thus have jacobian u: R^d -> R^{r x {d x rC}}, where
 *
 *                             |  (d u_s_1 / d x_1)(x),  (d u_s_1 / d x_2)(x), ...,  (d u_s_1 / d x_d)(x) |
 *                             |  (d u_s_2 / d x_1)(x),           .             ...           .           |
 *         (jacobian u)_s(x) = |           .                      .             ...           .           |
 *                             |           .                      .             ...           .           |
 *                             | (d u_s_rC / d x_1)(x), (d u_s_rC / d x_2)(x), ..., (d u_s_rC / d x_d)(x) |
 *
 *         Again, (d u_s_i / d x_j)(x) can be interpreted as the evaluation of a scalar function
 *         (jacobian u)_s_i_j: R^d -> R, and we can thus identify
 *
 *         (jacobian u)_s = ( d u_s_i / d x_j )_{i = 1, ..., rC | j = 1, ..., d}
 *
 *         This interpretation of us jacobian is used in DerivativeRangeTypeSelector, with a special case if rC = 1, to
 *         simplify manners.
 **/
template <class EntityImp, size_t rangeDim = 1, size_t rangeDimCols = 1, class RangeFieldImp = double>
class LocalFunctionSetInterface : public Common::ParametricInterface
{
  static_assert(XT::Grid::is_entity<EntityImp>::value, "");
  using ThisType = LocalFunctionSetInterface<EntityImp, rangeDim, rangeDimCols, RangeFieldImp>;

public:
  using EntityType = EntityImp;
  using DomainFieldType = double;
  static const constexpr size_t dimDomain = EntityType::dimension;
  using RangeFieldType = RangeFieldImp;
  static const constexpr size_t dimRange = rangeDim;
  static const constexpr size_t dimRangeCols = rangeDimCols;

  using E = EntityType;
  using D = DomainFieldType;
  static const constexpr size_t d = dimDomain;
  using R = RangeFieldType;
  static const constexpr size_t r = dimRange;
  static const constexpr size_t rC = dimRangeCols;

  using DomainType = Dune::FieldVector<D, d>;
  using RangeType = typename internal::RangeTypeSelector<R, r, rC>::type;
  using DerivativeRangeType = typename internal::DerivativeRangeTypeSelector<d, R, r, rC>::type;
  using SingleDerivativeRangeType = typename internal::DerivativeRangeTypeSelector<d, R, r, rC>::single_type;

  LocalFunctionSetInterface()
    : entity_(nullptr)
  {
  }

  LocalFunctionSetInterface(const EntityType& ent)
    : entity_(new EntityType(ent))
  {
  }

  LocalFunctionSetInterface(EntityType&& ent)
    : entity_(new EntityType(ent))
  {
  }

  LocalFunctionSetInterface(const ThisType& other)
    : entity_(nullptr)
  {
    if (other.entity_)
      entity_ = std::make_unique<EntityType>(*other.entity_);
  }

  LocalFunctionSetInterface(ThisType&& source)
    : entity_(std::move(source.entity_))
  {
  }

  virtual ~LocalFunctionSetInterface() = default;

  ThisType& operator=(const ThisType& other)
  {
    if (&other != this && other.entity_)
      entity_ = std::make_unique<EntityType>(other.entity_);
  }

  ThisType& operator=(ThisType&& source)
  {
    if (&source != this)
      entity_ = source.entity_;
  }

  /**
   * \attention The returned reference will change as soon as the funtion is bound to another entity!
   */
  const EntityType& entity() const
  {
    if (!entity_)
      DUNE_THROW(Exceptions::this_function_is_not_bound_to_an_entity_yet, "");
    return *entity_;
  }

  ThisType& bind(const EntityType& ent)
  {
    if (entity_ && ent == *entity_)
      return *this;
    entity_ = std::make_unique<EntityType>(ent);
    post_bind(*entity_);
    return *this;
  }

protected:
  /**
   * \note Override this function if you need/want to do preparatory work on an entity.
   */
  virtual void post_bind(const EntityType& /*ent*/)
  {
  }

public:
  /**
   * \name ´´These methods have to be implemented.''
   * \{
   **/

  virtual size_t size(const Common::Parameter& /*mu*/ = {}) const = 0;

  virtual int order(const Common::Parameter& /*mu*/ = {}) const = 0;

  /**
   * \}
   * \name ´´These methods should be implemented to define the functionality of the set of functions.''
   * \{
   **/

  virtual std::vector<RangeType> evaluate_set(const DomainType& /*xx*/, const Common::Parameter& /*mu*/ = {}) const
  {
    DUNE_THROW(NotImplemented,
               "This set of local functions does not provide evaluations, override the 'evaluate_set' method!");
  }

  virtual std::vector<DerivativeRangeType> jacobians_of_set(const DomainType& /*xx*/,
                                                            const Common::Parameter& /*mu*/ = {}) const
  {
    DUNE_THROW(NotImplemented,
               "This set of local functions does not provide jacobians, override the 'jacobians_of_set' method!");
  }

  virtual std::vector<DerivativeRangeType> derivatives_of_set(const std::array<size_t, d>& /*alpha*/,
                                                              const DomainType& /*xx*/,
                                                              const Common::Parameter& /*mu*/ = {}) const
  {
    DUNE_THROW(NotImplemented,
               "This set of local functions does not provide arbitrary derivatives, override the "
               "'derivatives_of_set' method!");
  }

  /**
   * \{
   * \name ´´These methods can be overridden to improve their performance.''
   * \{
   **/

  virtual std::vector<R>
  evaluate_set(const DomainType& xx, const size_t row, const size_t col, const Common::Parameter& mu = {}) const
  {
    ensure_correct_dims(row, col, "evaluate_set");
    return single_helper<R>::call(this->evaluate_set(xx, mu), row, col);
  }

  virtual std::vector<SingleDerivativeRangeType>
  jacobians_of_set(const DomainType& xx, const size_t row, const size_t col, const Common::Parameter& mu = {}) const
  {
    ensure_correct_dims(row, col, "jacobians_of_set");
    return single_helper<SingleDerivativeRangeType>::call(this->jacobians_of_set(xx, mu), row, col);
  }

  virtual std::vector<SingleDerivativeRangeType> derivatives_of_set(const std::array<size_t, d>& alpha,
                                                                    const DomainType& xx,
                                                                    const size_t row,
                                                                    const size_t col,
                                                                    const Common::Parameter& mu = {}) const
  {
    ensure_correct_dims(row, col, "derivatives_of_set");
    return single_helper<SingleDerivativeRangeType>::call(this->derivatives_of_set(alpha, xx, mu), row, col);
  }

  /**
   * \}
   **/
protected:
#ifndef DUNE_XT_FUNCTIONS_DISABLE_CHECKS
  void ensure_this_is_a_valid_point(const DomainType& xx) const
  {
    if (!ReferenceElements<D, d>::general(entity().type()).checkInside(xx)) {
      std::stringstream error_message;
      error_message << "This given point xx is not inside the current entity!"
                    << "\n\n";
      XT::Grid::print_entity(entity(), XT::Common::Typename<E>::value(), error_message, "   ");
      error_message << "\n   "
                    << "xx = " << xx << std::endl;
      DUNE_THROW(XT::Functions::Exceptions::wrong_input_given, error_message.str());
    }
  }
#else // DUNE_XT_FUNCTIONS_DISABLE_CHECKS
  void ensure_this_is_a_valid_point(const DomainType& /*xx*/) const
  {
  }
#endif

#ifndef DUNE_XT_FUNCTIONS_DISABLE_CHECKS
  static void ensure_correct_dims(const size_t row, const size_t col, const std::string& caller)
  {
    if (row >= r || col >= rC)
      DUNE_THROW(XT::Common::Exceptions::shapes_do_not_match,
                 "in " << caller << ": this function maps from " << d << " -> " << r << "x" << rC << "\n   "
                       << "and you are trying to access the entry in row "
                       << row
                       << " and column "
                       << col
                       << " of its range!");
  }
#else // DUNE_XT_FUNCTIONS_DISABLE_CHECKS
  static void ensure_correct_dims(const size_t /*row*/, const size_t /*col*/, const std::string& /*caller*/)
  {
  }
#endif

private:
  template <class SingleType, size_t _r = r, size_t _rC = rC, bool anything = true>
  struct single_helper
  {
    template <class FullType>
    static std::vector<SingleType> call(const std::vector<FullType>& val, const size_t row, const size_t col)
    {
      std::vector<SingleType> ret(val.size());
      for (size_t ii = 0; ii < val.size(); ++ii)
        for (size_t dd = 0; dd < d; ++dd)
          ret[ii][dd] = val[ii][row][col][dd];
      return ret;
    }
  }; // struct single_helper<r, rC, ...>

  template <class SingleType, size_t _r, bool anything>
  struct single_helper<SingleType, _r, 1, anything>
  {
    template <class FullType>
    static std::vector<SingleType> call(const std::vector<FullType>& val, const size_t row, const size_t /*col*/)
    {
      std::vector<SingleType> ret(val.size());
      for (size_t ii = 0; ii < val.size(); ++ii)
        ret[ii] = val[ii][row];
      return ret;
    }
  }; // struct single_helper<r, 1, ...>

  std::unique_ptr<EntityType> entity_;
}; // class LocalFunctionSetInterface


/**
 *  \brief  Interface for a globalvalued function, which can be evaluated locally on one Entity.
 */
template <class EntityImp, size_t rangeDim = 1, size_t rangeDimCols = 1, class RangeFieldImp = double>
class LocalFunctionInterface : public LocalFunctionSetInterface<EntityImp, rangeDim, rangeDimCols, RangeFieldImp>
{
  using BaseType = LocalFunctionSetInterface<EntityImp, rangeDim, rangeDimCols, RangeFieldImp>;
  using ThisType = LocalFunctionInterface<EntityImp, rangeDim, rangeDimCols, RangeFieldImp>;

public:
  using BaseType::d;
  using typename BaseType::R;
  using typename BaseType::DomainType;
  using typename BaseType::RangeType;
  using typename BaseType::DerivativeRangeType;
  using typename BaseType::SingleDerivativeRangeType;
  using typename BaseType::EntityType;

  LocalFunctionInterface()
    : BaseType()
  {
  }

  LocalFunctionInterface(const EntityType& ent)
    : BaseType(ent)
  {
  }

  LocalFunctionInterface(EntityType&& ent)
    : BaseType(ent)
  {
  }

  LocalFunctionInterface(const ThisType& other) = default;
  LocalFunctionInterface(ThisType&& source) = default;

  virtual ~LocalFunctionInterface() = default;

  ThisType& operator=(const ThisType& other) = default;
  ThisType& operator=(ThisType&& source) = default;

  using BaseType::evaluate_set;
  using BaseType::jacobians_of_set;
  using BaseType::derivatives_of_set;

  /**
   * \}
   * \name ´´These methods should be implemented to define the functionality of the function.''
   * \{
   **/

  virtual RangeType evaluate(const DomainType& /*xx*/, const Common::Parameter& /*mu*/ = {}) const
  {
    DUNE_THROW(NotImplemented, "This local function does not provide evaluations, override the 'evaluate' method!");
  }

  virtual DerivativeRangeType jacobian(const DomainType& /*xx*/, const Common::Parameter& /*mu*/ = {}) const
  {
    DUNE_THROW(NotImplemented, "This local function does not provide a jacobian, override the 'jacobian' method!");
  }

  virtual DerivativeRangeType derivative(const std::array<size_t, d>& /*alpha*/,
                                         const DomainType& /*xx*/,
                                         const Common::Parameter& /*mu*/ = {}) const
  {
    DUNE_THROW(NotImplemented,
               "This local function does not provide arbitrary derivatives, override the 'derivative' method!");
  }

  /**
   * \{
   * \name ´´These methods can be overridden to improve their performance.''
   * \{
   **/

  virtual R evaluate(const DomainType& xx, const size_t row, const size_t col, const Common::Parameter& mu = {}) const
  {
    ensure_correct_dims(row, col, "evaluate");
    return single_helper<R>::call(this->evaluate(xx, mu), row, col);
  }

  virtual SingleDerivativeRangeType
  jacobian(const DomainType& xx, const size_t row, const size_t col, const Common::Parameter& mu = {}) const
  {
    ensure_correct_dims(row, col, "jacobian");
    return single_helper<SingleDerivativeRangeType>::call(this->jacobian(xx, mu), row, col);
  }

  virtual SingleDerivativeRangeType derivative(const std::array<size_t, d>& alpha,
                                               const DomainType& xx,
                                               const size_t row,
                                               const size_t col = 0,
                                               const Common::Parameter& mu = {}) const
  {
    ensure_correct_dims(row, col, "derivative");
    return single_helper<SingleDerivativeRangeType>::call(this->derivative(alpha, xx, mu), row, col);
  }

  /**
   * \{
   * \name ´´These methods are required by LocalFunctionSetInterface and are provided by this interface.''
   * \{
   **/

  size_t size(const Common::Parameter& /*mu*/ = {}) const override final
  {
    return 1;
  }

  std::vector<RangeType> evaluate_set(const DomainType& xx, const Common::Parameter& mu = {}) const override final
  {
    return std::vector<RangeType>(1, evaluate(xx, mu));
  }

  std::vector<DerivativeRangeType> jacobians_of_set(const DomainType& xx,
                                                    const Common::Parameter& mu = {}) const override final
  {
    return std::vector<DerivativeRangeType>(1, jacobian(xx, mu));
  }

  std::vector<DerivativeRangeType> derivatives_of_set(const std::array<size_t, d>& alpha,
                                                      const DomainType& xx,
                                                      const Common::Parameter& mu = {}) const override final
  {
    return std::vector<DerivativeRangeType>(1, derivative(alpha, xx, mu));
  }

  /**
   * \}
   **/
private:
  using BaseType::ensure_correct_dims;

  template <class SingleType, size_t _r = BaseType::r, size_t _rC = BaseType::rC, bool anything = true>
  struct single_helper
  {
    template <class FullType>
    static SingleType call(const FullType& val, const size_t row, const size_t col)
    {
      return val[row][col];
    }
  }; // struct single_helper<r, rC, ...>

  template <class SingleType, size_t _r, bool anything>
  struct single_helper<SingleType, _r, 1, anything>
  {
    template <class FullType>
    static SingleType call(const FullType& val, const size_t row, const size_t /*col*/)
    {
      return val[row];
    }
  }; // struct single_helper<r, 1, ...>
}; // class LocalFunctionInterface


} // namespace Functions
} // namespace XT
} // namespace Dune

#endif // DUNE_XT_FUNCTIONS_INTERFACES_LOCAL_FUNCTIONS_HH
