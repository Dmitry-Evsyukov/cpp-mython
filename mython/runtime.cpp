#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (!object) return false;
    if (object.TryAs<Number>() != nullptr && object.TryAs<Number>()->GetValue() == 0) {
        return false;
    }
    if (object.TryAs<String>() != nullptr && object.TryAs<String>()->GetValue().empty()) {
        return false;
    }
    if (object.TryAs<Bool>() != nullptr && object.TryAs<Bool>()->GetValue() == false) {
        return false;
    }
    if (object.TryAs<Bool>() == nullptr && object.TryAs<String>() == nullptr && object.TryAs<Number>() == nullptr) {
        return false;
    }
    return true;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__", 0)) {
        Call("__str__", {}, context)->Print(os, context);
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    auto meth = cls_.TryAs<Class>()->GetMethod(method);
    if (meth != nullptr && meth->formal_params.size() == argument_count) {
        return true;
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return fields_;
}

const Closure& ClassInstance::Fields() const {
    return fields_;
}

ClassInstance::ClassInstance(Class& cls) {
    cls_ = move(ObjectHolder::Share(cls));
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (!HasMethod(method, actual_args.size())) {
        throw std::runtime_error("Not implemented"s);
    }
    auto meth = cls_.TryAs<Class>()->GetMethod(method);
    Closure function_args;
    function_args["self"] = ObjectHolder::Share(*this);
    for (size_t i = 0; i < actual_args.size(); ++ i) {
        if (meth->formal_params[i] == "self") continue;
        function_args[meth->formal_params[i]] = actual_args[i];
    }
    if (meth->body) {
        return meth->body->Execute(function_args, context);
    }
    throw std::runtime_error("Not implemented"s);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent) : name_(name), methods_(move(methods)), parent_(parent){ }

const Method* Class::GetMethod(const std::string& name) const {
    for (const auto& method : methods_) {
        if (method.name == name) {
            return &method;
        }
    }
    if (parent_ == nullptr) {
        return nullptr;
    } else {
        return parent_->GetMethod(name);
    }

}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, Context& /*context*/) {
    os << "Class " << GetName();
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.Get() != nullptr && rhs.Get() != nullptr) {
        if (lhs.TryAs<Bool>() != nullptr && rhs.TryAs<Bool>() != nullptr) {
            return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
        }
        if (lhs.TryAs<Number>() != nullptr && rhs.TryAs<Number>() != nullptr) {
            return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
        }
        if (lhs.TryAs<String>() != nullptr && rhs.TryAs<String>() != nullptr) {
            return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
        }
        if (lhs.TryAs<ClassInstance>() != nullptr && rhs.TryAs<ClassInstance>() != nullptr && lhs.TryAs<ClassInstance>()->HasMethod("__eq__", 1)) {
            return lhs.TryAs<ClassInstance>()->Call("__eq__", {rhs}, context).TryAs<Bool>()->GetValue();
        }
    }
    if (!lhs && !rhs) {
        return true;
    }
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.Get() != nullptr && rhs.Get() != nullptr) {
        if (lhs.TryAs<Bool>() != nullptr && rhs.TryAs<Bool>() != nullptr) {
            return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
        }
        if (lhs.TryAs<Number>() != nullptr && rhs.TryAs<Number>() != nullptr) {
            return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
        }
        if (lhs.TryAs<String>() != nullptr && rhs.TryAs<String>() != nullptr) {
            return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
        }
        if (lhs.TryAs<ClassInstance>() != nullptr && lhs.TryAs<ClassInstance>()->HasMethod("__lt__", 1)) {
            return lhs.TryAs<ClassInstance>()->Call("__lt__", {rhs}, context).TryAs<Bool>()->GetValue();
        }
    }

       throw std::runtime_error("Cannot compare objects for less"s);
}



bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return GreaterOrEqual(lhs, rhs, context) && NotEqual(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Greater(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime