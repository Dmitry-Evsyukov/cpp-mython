#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        auto val = rv_->Execute(closure, context);
        closure[var_] = val;
        return closure[var_];
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) : var_(var), rv_(std::move(rv)) {}

    VariableValue::VariableValue(const std::string& var_name) {
        dotted_ids_.push_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids) : dotted_ids_(std::move(dotted_ids)) {}

    ObjectHolder VariableValue::Execute(Closure& closure, Context& context) {
        if (closure.count(dotted_ids_[0]) == 0) throw runtime_error("");
        if (dotted_ids_.size() == 1) {
            return closure[dotted_ids_[0]];
        } else {
            std::vector<std::string> new_closure (dotted_ids_.begin() + 1, dotted_ids_.end());
            return VariableValue(new_closure).Execute(closure[dotted_ids_[0]].TryAs<runtime::ClassInstance>()->Fields(),context);
        }
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        std::unique_ptr<Statement> arg = make_unique<VariableValue>(VariableValue(name));
        return make_unique<Print>(std::move(arg));
    }


    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) : args_(std::move(args)) {}

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        if (args_.empty()) {
            context.GetOutputStream() << std::endl;
            return {};
        }
        auto result = args_[0]->Execute(closure, context);
        if (result.Get() == nullptr) {
            context.GetOutputStream() << "None";
        } else {
            result->Print(context.GetOutputStream(), context);
        }
        for (size_t i = 1; i < args_.size(); ++i) {
            context.GetOutputStream() << ' ';
            auto result_in_vector = args_[i]->Execute(closure, context);
            if (result_in_vector.Get() == nullptr) {
                context.GetOutputStream() << "None";
            } else {
                result_in_vector->Print(context.GetOutputStream(), context);
            }

        }
        context.GetOutputStream() << std::endl;
        return result;
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                           std::vector<std::unique_ptr<Statement>> args) : object_(std::move(object)), method_(method), args_(std::move(args)) {}

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        std::vector<ObjectHolder> actual_args;
        for (const auto & arg : args_) {
            actual_args.push_back(arg->Execute(closure, context));
        }
        return object_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(method_, actual_args, context);
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        ostringstream ss;
        if (arg_->Execute(closure, context)) {
            arg_->Execute(closure, context)->Print(ss, context);
        }else {
            ss << "None";
        }

        runtime::String str(ss.str());
        return ObjectHolder::Own(std::move(str));
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() && rhs_->Execute(closure, context).TryAs<runtime::Number>()) {
            auto num1 = lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto num2 = rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto sum = num1 + num2;
            runtime::Number answer(sum);
            return ObjectHolder::Own(std::move(answer));
        } else if (lhs_->Execute(closure, context).TryAs<runtime::String>() && rhs_->Execute(closure, context).TryAs<runtime::String>()) {
            auto str1 = lhs_->Execute(closure, context).TryAs<runtime::String>()->GetValue();
            auto str2 = rhs_->Execute(closure, context).TryAs<runtime::String>()->GetValue();
            auto str_concat = str1 + str2;
            runtime::String answer(str_concat);
            return ObjectHolder::Own(std::move(answer));
        } else if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>() && lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1)) {
            return lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, {rhs_->Execute(closure, context)}, context);
        } else {
            throw runtime_error("");
        }
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() && rhs_->Execute(closure, context).TryAs<runtime::Number>()) {
            auto num1 = lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto num2 = rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto sum = num1 - num2;
            runtime::Number answer(sum);
            return ObjectHolder::Own(std::move(answer));
        } else {
            throw runtime_error("");
        }
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() && rhs_->Execute(closure, context).TryAs<runtime::Number>()) {
            auto num1 = lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto num2 = rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto sum = num1 * num2;
            runtime::Number answer(sum);
            return ObjectHolder::Own(std::move(answer));
        } else {
            throw runtime_error("");
        }
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() && rhs_->Execute(closure, context).TryAs<runtime::Number>()) {
            auto num1 = lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            auto num2 = rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue();
            if (num2 == 0) {throw runtime_error("");}
            auto div = num1 / num2;
            runtime::Number answer(div);
            return ObjectHolder::Own(std::move(answer));
        } else {
            throw runtime_error("");
        }
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (const auto& arg : args_) {
            arg->Execute(closure, context);
        }
        return {};
    }

    namespace {
        class ReturnExcept : exception {
        public:
            ReturnExcept(ObjectHolder obj) : answer_(std::move(obj)) {}
            ObjectHolder GetResponse() const {
                return answer_;
            }
        private:
            ObjectHolder answer_;
        };
    }
    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        auto result = statement_->Execute(closure, context);
        throw ReturnExcept(std::move(result));
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(std::move(cls)) { }

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
        auto class_name = cls_.TryAs<runtime::Class>()->GetName();
        closure[class_name] = cls_;
        return closure[class_name];
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                     std::unique_ptr<Statement> rv) : obj_(object), field_name_(field_name), rv_(std::move(rv)) {}

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        if (obj_.Execute(closure,context).TryAs<runtime::ClassInstance>() != nullptr) {
            obj_.Execute(closure,context).TryAs<runtime::ClassInstance>()->Fields()[field_name_] = rv_->Execute(closure, context);
        } else {
            throw runtime_error("");
        }
        return obj_.Execute(closure,context).TryAs<runtime::ClassInstance>()->Fields()[field_name_];
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body) : condition_(std::move(condition)), if_body_(std::move(if_body)), else_body_(std::move(else_body)){}

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        } else {
            if (else_body_ != nullptr) {
                return else_body_->Execute(closure, context);
            }
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        bool lhs_bool = IsTrue(lhs_->Execute(closure, context));

        if (lhs_bool) {
            runtime::Bool answer(lhs_bool);
            return ObjectHolder::Own(std::move(answer));
        } else {
            runtime::Bool answer(runtime::IsTrue(rhs_->Execute(closure, context)));
            return ObjectHolder::Own(std::move(answer));
        }
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        bool lhs_bool = IsTrue(lhs_->Execute(closure, context));

        if (!lhs_bool) {
            runtime::Bool answer(lhs_bool);
            return ObjectHolder::Own(std::move(answer));
        } else {
            runtime::Bool answer(runtime::IsTrue(rhs_->Execute(closure, context)));
            return ObjectHolder::Own(std::move(answer));
        }
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        runtime::Bool answer(!runtime::IsTrue(arg_->Execute(closure, context)));
        return ObjectHolder::Own(std::move(answer));
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
            : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp) { }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        auto result = cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context);
        runtime::Bool result_bool(result);
        return ObjectHolder::Own(std::move(result_bool));

    }

    NewInstance::NewInstance(runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) : new_instance_(class_), args_(std::move(args)) {}

    NewInstance::NewInstance(runtime::Class& class_) : new_instance_(class_) {}

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        if (new_instance_.HasMethod(INIT_METHOD, args_.size())) {
            std::vector<ObjectHolder> actual_args;
            for (const auto& arg : args_) {
                actual_args.push_back(arg->Execute(closure, context));
            }
            new_instance_.Call(INIT_METHOD, actual_args, context);
        }
        return ObjectHolder::Share(new_instance_);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body)) { }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
        } catch (ReturnExcept& result) {
            return result.GetResponse();
        }
        return {};
    }

}  // namespace ast