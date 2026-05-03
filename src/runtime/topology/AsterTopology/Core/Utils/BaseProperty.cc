#include <AsterTopology/Core/Utils/BaseProperty.hh>

namespace AsterTopology
{

void BaseProperty::stats(std::ostream& _ostr) const
{
  _ostr << "  " << name() << (persistent() ? ", persistent " : "") << "\n";
}

}
