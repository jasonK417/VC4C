/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "IntermediateInstruction.h"

#include "../asm/LoadInstruction.h"
#include "../helper.h"
#include "../periphery/VPM.h"
#include "log.h"

#include <bitset>
#include <cstdbool>
#include <memory>

using namespace vc4c;
using namespace vc4c::intermediate;

LoadImmediate::LoadImmediate(
    const Value& dest, const Literal& source, const ConditionCode& cond, const SetFlag setFlags) :
    IntermediateInstruction(dest, cond, setFlags),
    type(LoadType::REPLICATE_INT32)
{
    // 32-bit integers are loaded through all SIMD-elements!
    // "[...] write either a 32-bit immediate across the entire SIMD array" (p. 33)
    setImmediate(source);
}

LoadImmediate::LoadImmediate(
    const Value& dest, uint32_t mask, LoadType type, const ConditionCode& cond, SetFlag setFlags) :
    IntermediateInstruction(dest, cond, setFlags),
    type(type)
{
    setArgument(0, Value(Literal(mask), TYPE_INT32));
}

std::vector<vc4c::Value> toLoadedValues(uint32_t mask, vc4c::intermediate::LoadType type)
{
    std::bitset<32> bits(mask);
    std::vector<Value> values;
    values.reserve(16);
    if(type == LoadType::PER_ELEMENT_UNSIGNED)
    {
        for(std::size_t i = 0; i < 16; ++i)
        {
            values.emplace_back(
                Literal(static_cast<unsigned>(bits.test(i + 16)) * 2u + static_cast<unsigned>(bits.test(i))),
                TYPE_INT8);
        }
    }
    else if(type == LoadType::PER_ELEMENT_SIGNED)
    {
        for(std::size_t i = 0; i < 16; ++i)
        {
            values.emplace_back(
                Literal(static_cast<int>(bits.test(i + 16)) * -2 + static_cast<int>(bits.test(i))), TYPE_INT32);
        }
    }
    else
        throw CompilationError(
            CompilationStep::GENERAL, "Unhandled type of masked load", std::to_string(static_cast<unsigned>(type)));
    return values;
}

std::string LoadImmediate::to_string() const
{
    switch(type)
    {
    case LoadType::REPLICATE_INT32:
        if(getOutput()->hasRegister(REG_VPM_IN_SETUP) ||
            has_flag(decoration, InstructionDecorations::VPM_READ_CONFIGURATION))
            return (getOutput()->to_string(true) + " = loadi ") +
                periphery::VPRSetup::fromLiteral(getImmediate().unsignedInt()).to_string() +
                createAdditionalInfoString();
        if(getOutput()->hasRegister(REG_VPM_OUT_SETUP) ||
            has_flag(decoration, InstructionDecorations::VPM_WRITE_CONFIGURATION))
            return (getOutput()->to_string(true) + " = loadi ") +
                periphery::VPWSetup::fromLiteral(getImmediate().unsignedInt()).to_string() +
                createAdditionalInfoString();
        return (getOutput()->to_string(true) + " = loadi ") + assertArgument(0).to_string() +
            createAdditionalInfoString();
    case LoadType::PER_ELEMENT_SIGNED:

        return (getOutput()->to_string(true) + " = loadsi ") +
            vc4c::to_string<Value>(
                toLoadedValues(assertArgument(0).literal().unsignedInt(), LoadType::PER_ELEMENT_SIGNED)) +
            createAdditionalInfoString();
    case LoadType::PER_ELEMENT_UNSIGNED:
        return (getOutput()->to_string(true) + " = loadui ") +
            vc4c::to_string<Value>(
                toLoadedValues(assertArgument(0).literal().unsignedInt(), LoadType::PER_ELEMENT_UNSIGNED)) +
            createAdditionalInfoString();
    }
    throw CompilationError(
        CompilationStep::GENERAL, "Unhandled type of load", std::to_string(static_cast<unsigned>(type)));
}

IntermediateInstruction* LoadImmediate::copyFor(Method& method, const std::string& localPrefix) const
{
    switch(type)
    {
    case LoadType::REPLICATE_INT32:
        return (new LoadImmediate(renameValue(method, getOutput().value(), localPrefix), assertArgument(0).literal(),
                    conditional, setFlags))
            ->copyExtrasFrom(this);
    case LoadType::PER_ELEMENT_SIGNED:
    case LoadType::PER_ELEMENT_UNSIGNED:
        return (new LoadImmediate(renameValue(method, getOutput().value(), localPrefix),
                    assertArgument(0).literal().unsignedInt(), type, conditional, setFlags))
            ->copyExtrasFrom(this);
    }
    throw CompilationError(
        CompilationStep::GENERAL, "Unhandled type of load", std::to_string(static_cast<unsigned>(type)));
}

Optional<Value> LoadImmediate::precalculate(const std::size_t numIterations) const
{
    switch(type)
    {
    case LoadType::REPLICATE_INT32:
        return getArgument(0);
    case LoadType::PER_ELEMENT_SIGNED:
        return Value(ContainerValue(toLoadedValues(assertArgument(0).literal().unsignedInt(), type)),
            TYPE_INT32.toVectorType(16));
    case LoadType::PER_ELEMENT_UNSIGNED:
        return Value(ContainerValue(toLoadedValues(assertArgument(0).literal().unsignedInt(), type)),
            TYPE_INT8.toVectorType(16));
    }
    throw CompilationError(
        CompilationStep::GENERAL, "Unhandled type of load", std::to_string(static_cast<unsigned>(type)));
}

qpu_asm::Instruction* LoadImmediate::convertToAsm(const FastMap<const Local*, Register>& registerMapping,
    const FastMap<const Local*, std::size_t>& labelMapping, const std::size_t instructionIndex) const
{
    const Register outReg = getOutput()->hasRegister() ? getOutput()->reg() : registerMapping.at(getOutput()->local());
    const ConditionCode conditional0 = outReg.num == REG_NOP.num ? COND_NEVER : this->conditional;
    auto res = new qpu_asm::LoadInstruction(PACK_NOP, conditional0, COND_NEVER, setFlags,
        outReg.file == RegisterFile::PHYSICAL_A ? WriteSwap::DONT_SWAP : WriteSwap::SWAP, outReg.num, REG_NOP.num,
        assertArgument(0).literal().toImmediate());
    switch(type)
    {
    case LoadType::REPLICATE_INT32:
        res->setType(OpLoad::LOAD_IMM_32);
        break;
    case LoadType::PER_ELEMENT_SIGNED:
        res->setType(OpLoad::LOAD_SIGNED);
        break;
    case LoadType::PER_ELEMENT_UNSIGNED:
        res->setType(OpLoad::LOAD_UNSIGNED);
        break;
    default:
        throw CompilationError(
            CompilationStep::GENERAL, "Unhandled type of load", std::to_string(static_cast<unsigned>(type)));
    }

    return res;
}

bool LoadImmediate::isNormalized() const
{
    return true;
}

Literal LoadImmediate::getImmediate() const
{
    if(type != LoadType::REPLICATE_INT32)
        throw CompilationError(CompilationStep::GENERAL, "Cannot query immediate value from masked load", to_string());
    return assertArgument(0).literal();
}

void LoadImmediate::setImmediate(const Literal& value)
{
    if(type != LoadType::REPLICATE_INT32)
        throw CompilationError(CompilationStep::GENERAL, "Cannot set immediate value for masked load", to_string());
    setArgument(0,
        Value(value,
            value.type == LiteralType::INTEGER ? TYPE_INT32 :
                                                 (value.type == LiteralType::REAL ? TYPE_FLOAT : TYPE_BOOL)));
}
