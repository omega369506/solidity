/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0


#include <libsolidity/experimental/analysis/TypeClassMemberRegistration.h>

#include <libsolidity/experimental/analysis/Analysis.h>
#include <libsolidity/experimental/analysis/TypeClassRegistration.h>
#include <libsolidity/experimental/analysis/TypeRegistration.h>
#include <libsolidity/experimental/ast/TypeSystemHelper.h>

#include <liblangutil/ErrorReporter.h>
#include <liblangutil/Exceptions.h>

using namespace solidity::frontend::experimental;
using namespace solidity::langutil;

TypeClassMemberRegistration::TypeClassMemberRegistration(Analysis& _analysis):
	m_analysis(_analysis),
	m_errorReporter(_analysis.errorReporter()),
	m_typeSystem(_analysis.typeSystem())
{
	TypeSystemHelpers helper{m_typeSystem};

	auto registeredTypeClass = [&](BuiltinClass _builtinClass) -> TypeClass {
		return m_analysis.annotation<TypeClassRegistration>().builtinClasses.at(_builtinClass);
	};

	auto defineConversion = [&](BuiltinClass _builtinClass, PrimitiveType _fromType, std::string _functionName) {
		annotation().typeClassFunctions[registeredTypeClass(_builtinClass)] = {{
			std::move(_functionName),
			helper.functionType(
				m_typeSystem.type(_fromType, {}),
				m_typeSystem.typeClassInfo(registeredTypeClass(_builtinClass)).typeVariable
			),
		}};
	};

	auto defineBinaryMonoidalOperator = [&](BuiltinClass _builtinClass, Token _token, std::string _functionName) {
		Type typeVar = m_typeSystem.typeClassInfo(registeredTypeClass(_builtinClass)).typeVariable;
		annotation().operators.emplace(_token, std::make_tuple(registeredTypeClass(_builtinClass), _functionName));
		annotation().typeClassFunctions[registeredTypeClass(_builtinClass)] = {{
			std::move(_functionName),
			helper.functionType(
				helper.tupleType({typeVar, typeVar}),
				typeVar
			)
		}};
	};

	auto defineBinaryCompareOperator = [&](BuiltinClass _builtinClass, Token _token, std::string _functionName) {
		Type typeVar = m_typeSystem.typeClassInfo(registeredTypeClass(_builtinClass)).typeVariable;
		annotation().operators.emplace(_token, std::make_tuple(registeredTypeClass(_builtinClass), _functionName));
		annotation().typeClassFunctions[registeredTypeClass(_builtinClass)] = {{
			std::move(_functionName),
			helper.functionType(
				helper.tupleType({typeVar, typeVar}),
				m_typeSystem.type(PrimitiveType::Bool, {})
			)
		}};
	};

	defineConversion(BuiltinClass::Integer, PrimitiveType::Integer, "fromInteger");

	defineBinaryMonoidalOperator(BuiltinClass::Mul, Token::Mul, "mul");
	defineBinaryMonoidalOperator(BuiltinClass::Add, Token::Add, "add");

	defineBinaryCompareOperator(BuiltinClass::Equal, Token::Equal, "eq");
	defineBinaryCompareOperator(BuiltinClass::Less, Token::LessThan, "lt");
	defineBinaryCompareOperator(BuiltinClass::LessOrEqual, Token::LessThanOrEqual, "leq");
	defineBinaryCompareOperator(BuiltinClass::Greater, Token::GreaterThan, "gt");
	defineBinaryCompareOperator(BuiltinClass::GreaterOrEqual, Token::GreaterThanOrEqual, "geq");
}

bool TypeClassMemberRegistration::analyze(SourceUnit const& _sourceUnit)
{
	_sourceUnit.accept(*this);
	return !m_errorReporter.hasErrors();
}

void TypeClassMemberRegistration::endVisit(TypeClassDefinition const& _typeClassDefinition)
{
	solAssert(m_analysis.annotation<TypeClassRegistration>(_typeClassDefinition).typeClass.has_value());
	TypeClass typeClass = m_analysis.annotation<TypeClassRegistration>(_typeClassDefinition).typeClass.value();

	_typeClassDefinition.typeVariable().accept(*this);

	std::map<std::string, Type> functionTypes;
	for (auto subNode: _typeClassDefinition.subNodes())
	{
		auto const* functionDefinition = dynamic_cast<FunctionDefinition const*>(subNode.get());
		solAssert(functionDefinition);

		std::optional<Type> functionType = TypeSystemHelpers{m_typeSystem}.functionType(
			m_typeSystem.freshTypeVariable({}),
			m_typeSystem.freshTypeVariable({})
		);

		if (!functionTypes.emplace(functionDefinition->name(), functionType.value()).second)
			m_errorReporter.fatalTypeError(
				3195_error,
				// TODO: Secondary location with previous definition
				functionDefinition->location(),
				"Function in type class declared multiple times."
			);
	}

	annotation().typeClassFunctions[typeClass] = std::move(functionTypes);
}

TypeClassMemberRegistration::Annotation& TypeClassMemberRegistration::annotation(ASTNode const& _node)
{
	return m_analysis.annotation<TypeClassMemberRegistration>(_node);
}

TypeClassMemberRegistration::Annotation const& TypeClassMemberRegistration::annotation(ASTNode const& _node) const
{
	return m_analysis.annotation<TypeClassMemberRegistration>(_node);
}

TypeClassMemberRegistration::GlobalAnnotation& TypeClassMemberRegistration::annotation()
{
	return m_analysis.annotation<TypeClassMemberRegistration>();
}
