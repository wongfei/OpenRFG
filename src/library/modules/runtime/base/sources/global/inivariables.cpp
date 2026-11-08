// Original: library/modules/runtime/base/sources/global/inivariables.cpp
//
// The registry of ini variables: an intrusive list (IniVariableList) that every IniVariableBase joins on construction.
//
// Partial: initialize, shutdown and destroyAllVariables (GameFramework's start-up and shutdown). Not ported: the variable
// classes, add/remove/find, the file reading and writing and the modification callback; no code that registers
// variables is ported yet, so the list stays empty.

#include "keen/base/inivariables.hpp"
#include "library/modules/runtime/base/sources/global/inivariablelist.hpp"

namespace keen
{
    // inlined at its callers
    static IniVariableList& getVariableList()
    {
        static IniVariableList s_variableList; // 0x038B3D40
        return s_variableList;
    }

    namespace IniVariables
    {
        // 0x00BF08D0 (:58)
        void initialize()
        {
        }

        // 0x00BFE6C0 (:62)
        // Each variable is destroyed, then the list is emptied (GameFramework::shutdown).
        void shutdown()
        {
            IniVariableList& variableList = getVariableList();
            for (IniVariableBase* pVariable = variableList.getHead(); pVariable != nullptr; pVariable = variableList.getNext(pVariable))
            {
                pVariable->destroy();
            }
            variableList.clear();
        }

        // 0x00BF86F0 (:117)
        void destroyAllVariables()
        {
            IniVariableList& variableList = getVariableList();
            for (IniVariableBase* pVariable = variableList.getHead(); pVariable != nullptr; pVariable = variableList.getNext(pVariable))
            {
                pVariable->destroy();
            }
            variableList.clear();
        }
    }
}
