#include "SSAApplyRegRef.h"



namespace holodec {

	bool SSAApplyRegRef::doTransformation(Binary* binary, Function* function) {
		bool applied = false;
		function->regStates.states.clear();
		for (SSAExpression& expr : function->ssaRep.expressions) {
			if (expr.type == SSAExprType::eReturn) {
				for (SSAArgument& arg : expr.subExpressions) {
					if (arg.type == SSAArgType::eId) {
						SSAExpression& reffedExpr = function->ssaRep.expressions[arg.ssaId];
						if (reffedExpr.type == SSAExprType::eInput && arg.location == SSALocation::eReg) {
							Register* reg = arch->getRegister(arg.locref.refId);
							RegisterState* state = function->regStates.getNewRegisterState(reg->parentRef.refId);
							state->arithChange = arg.valueoffset;
							continue;
						}
					}
					if (arg.location == SSALocation::eReg) {
						Register* reg = arch->getRegister(arg.locref.refId);
						RegisterState* state = function->regStates.getNewRegisterState(reg->parentRef.refId);
						state->flags |= RegisterUsedFlag::eWrite;
					}
				}
			}
			else if (expr.type == SSAExprType::eInput) {
				if (expr.location == SSALocation::eReg) {
					Register* reg = arch->getRegister(expr.locref.refId);
					RegisterState* state = function->regStates.getNewRegisterState(reg->parentRef.refId);
					state->flags |= RegisterUsedFlag::eRead;
				}
			}
			else if (expr.type == SSAExprType::eCall) {
				if (expr.subExpressions[0].type != SSAArgType::eUInt)
					continue;
				Function* callFunc = binary->getFunctionByAddr(expr.subExpressions[0].uval);
				if (!(callFunc && callFunc->regStates.parsed))
					continue;
				for (auto it = expr.subExpressions.begin(); it != expr.subExpressions.end(); ++it) {
					if (it->location != SSALocation::eReg)
						continue;
					Register* reg = arch->getRegister(it->locref.refId);
					if (!reg)
						continue;
					RegisterState* state = callFunc->regStates.getRegisterState(reg->parentRef.refId);
					if (!state || !state->flags.contains(RegisterUsedFlag::eRead)) {
						it = expr.subExpressions.erase(it) - 1;
						applied = true;
						continue;
					}
				}
			}
			else if (expr.type == SSAExprType::eOutput) {
				if (expr.subExpressions[0].type != SSAArgType::eId || expr.location != SSALocation::eReg)
					continue;
				SSAExpression& callExpr = function->ssaRep.expressions[expr.subExpressions[0].ssaId];
				if (callExpr.subExpressions[0].type != SSAArgType::eUInt)
					continue;
				Function* callFunc = binary->getFunctionByAddr(callExpr.subExpressions[0].uval);
				if (!(callFunc && callFunc->regStates.parsed))
					continue;
				Register* reg = arch->getRegister(expr.locref.refId);
				if (!reg)
					continue;
				RegisterState* state = callFunc->regStates.getRegisterState(reg->parentRef.refId);
				if (!state || !state->flags.contains(RegisterUsedFlag::eWrite)) {
					if (state && state->arithChange) {
						expr.type = SSAExprType::eOp;
						expr.subExpressions.erase(expr.subExpressions.begin());
						if (state->arithChange > 0) {
							expr.opType = SSAOpType::eAdd;
							expr.addArgument(&function->ssaRep, SSAArgument::createUVal(state->arithChange, arch->bitbase));
						}
						else {
							expr.opType = SSAOpType::eSub;
							expr.addArgument(&function->ssaRep, SSAArgument::createUVal(state->arithChange * -1, arch->bitbase));
						}
						applied = true;
					}
					else {
						expr.type = SSAExprType::eAssign;
						if (expr.uniqueId == 0x2e)
							printf("");
						expr.subExpressions.erase(expr.subExpressions.begin());
						applied = true;
					}
				}
				else if (!state || !state->flags.contains(RegisterUsedFlag::eRead)) {
					if (expr.subExpressions.size() > 1) {
						expr.subExpressions.erase(expr.subExpressions.begin() + 1);
						applied = true;
					}
				}
			}
		}
		function->regStates.parsed = true;
		return applied;
	}


}

