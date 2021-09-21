/*
* Tencent is pleased to support the open source community by making Puerts available.
* Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
* Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may be subject to their corresponding license terms.
* This file is subject to the terms and conditions defined in file 'LICENSE', which is part of this source code package.
*/

#include "ContainerWrapper.h"
#include "PropertyTranslator.h"

namespace puerts
{
    FORCEINLINE static int32 GetKeyOffset(const FScriptMapLayout &ScriptMapLayout)
    {
#if ENGINE_MINOR_VERSION < 22 && ENGINE_MAJOR_VERSION < 5
        return ScriptMapLayout.KeyOffset;
#else
        return 0;
#endif
    }

    v8::Local<v8::FunctionTemplate> FScriptArrayWrapper::ToFunctionTemplate(v8::Isolate* Isolate)
    {
        v8::Isolate::Scope Isolatescope(Isolate);
        v8::EscapableHandleScope HandleScope(Isolate);
        auto Result = v8::FunctionTemplate::New(Isolate, New);
        Result->InstanceTemplate()->SetInternalFieldCount(4);//0 Ptr, 1 Property

        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Num"), v8::FunctionTemplate::New(Isolate, Num));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Add"), v8::FunctionTemplate::New(Isolate, Add));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Get"), v8::FunctionTemplate::New(Isolate, Get));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Set"), v8::FunctionTemplate::New(Isolate, Set));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Contains"), v8::FunctionTemplate::New(Isolate, Contains));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "FindIndex"), v8::FunctionTemplate::New(Isolate, FindIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "RemoveAt"), v8::FunctionTemplate::New(Isolate, RemoveAt));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "IsValidIndex"), v8::FunctionTemplate::New(Isolate, IsValidIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Empty"), v8::FunctionTemplate::New(Isolate, Empty));

        return HandleScope.Escape(Result);
    }

    void FScriptArrayWrapper::Add(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        if (Info.Length() > 0)
        {
            auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
            auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
            if (!Inner->PropertyWeakPtr.IsValid())
            {
                FV8Utils::ThrowException(Isolate, "item info is invalid!");
                return;
            }
            
            int32 Index = AddUninitialized(Self, Inner->Property->GetSize(), Info.Length());
            for (int i = 0; i < Info.Length(); ++i)
            {
                uint8 *DataPtr = GetData(Self, Inner->Property->GetSize(), Index + i);
                Inner->Property->InitializeValue(DataPtr);//使用之前必须得初始化，即使是设置也要
                Inner->JsToUE(Isolate, Context, Info[i], DataPtr, false);
            }
            Info.GetReturnValue().Set(Index);
        }
    }

    void FScriptArrayWrapper::Get(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
            
        int Index = Info[0]->Int32Value(Context).ToChecked();
        if (Index < 0 || Index >= Self->Num())
        {
            FV8Utils::ThrowException(Isolate, TEXT("invalid index"));
            return;
        }
        uint8 *DataPtr = GetData(Self, Inner->Property->GetSize(), Index);
        Info.GetReturnValue().Set(Inner->UEToJs(Isolate, Context, DataPtr, true));
    }

    void FScriptArrayWrapper::Set(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(2);
        
        auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        
        int Index = Info[0]->Int32Value(Context).ToChecked();
        if (Index < 0 || Index >= Self->Num())
        {
            FV8Utils::ThrowException(Isolate, TEXT("invalid index"));
            return;
        }
        uint8 *DataPtr = GetData(Self, Inner->Property->GetSize(), Index);
        Inner->Property->InitializeValue(DataPtr);
        Inner->JsToUE(Isolate, Context, Info[1], DataPtr, false);
    }

    void FScriptArrayWrapper::Contains(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        CHECK_V8_ARGS_LEN(1);
        bool Result = FindIndexInner(Info) != INDEX_NONE;
        Info.GetReturnValue().Set(Result);
    }

    void FScriptArrayWrapper::FindIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        CHECK_V8_ARGS_LEN(1);
        int32 Result = FindIndexInner(Info);
        Info.GetReturnValue().Set(Result);
    }

    void FScriptArrayWrapper::RemoveAt(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);
        
        auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }

        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        if (Index < 0 || Index >= Self->Num())
        {
            FV8Utils::ThrowException(Isolate, TEXT("invalid index"));
            return;
        }
        else
        {
            Destruct(Self, Inner, Index, 1);
            Self->Remove(Index, 1, Inner->Property->GetSize());
        }
    }

    void FScriptArrayWrapper::IsValidIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        Info.GetReturnValue().Set(Self->IsValidIndex(Index));
    }

    void FScriptArrayWrapper::Empty(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }

        Destruct(Self, Inner, 0, Self->Num());
        Self->Empty(0, Inner->Property->GetSize());
    }

    FORCEINLINE int32 FScriptArrayWrapper::AddUninitialized(FScriptArray *ScriptArray, int32 ElementSize, int32 Count)
    {
        return ScriptArray->Add(Count, ElementSize);
    }

    FORCEINLINE uint8* FScriptArrayWrapper::GetData(FScriptArray *ScriptArray, int32 ElementSize, int32 Index)
    {
        return reinterpret_cast<uint8*>(ScriptArray->GetData()) + Index * ElementSize;
    }

    FORCEINLINE void FScriptArrayWrapper::Construct(FScriptArray *ScriptArray, FPropertyTranslator *Inner, int32 Index, int32 Count)
    {
        int32 ElementSize = Inner->Property->GetSize();
        uint8 *Dest = GetData(ScriptArray, ElementSize, Index);
        for (int32 i = 0; i < Count; ++i)
        {
            Inner->Property->InitializeValue(Dest);
            Dest += ElementSize;
        }
    }

    FORCEINLINE void FScriptArrayWrapper::Destruct(FScriptArray *ScriptArray, FPropertyTranslator *Inner, int32 Index, int32 Count)
    {
        int32 ElementSize = Inner->Property->GetSize();
        uint8 *Dest = GetData(ScriptArray, ElementSize, Index);
        for (int32 i = 0; i < Count; ++i)
        {
            Inner->Property->DestroyValue(Dest);
            Dest += ElementSize;
        }
    }

    int32 FScriptArrayWrapper::FindIndexInner(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptArray>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return INDEX_NONE;
        }
        auto Property = Inner->Property;

        void* Dest = FMemory_Alloca(Property->GetSize());
        Property->InitializeValue(Dest);
        Inner->JsToUE(Isolate, Context, Info[0], Dest, false);

        const int32 Num = Self->Num();
        int32 Result = INDEX_NONE;
        for (int32 i = 0; i < Num; ++i)
        {
            uint8* Src = GetData(Self, Property->GetSize(), i);
            if (Property->Identical(Src, Dest))
            {
                Result = i;
                break;
            }
        }
        Property->DestroyValue(Dest);
        return Result;
    }

    //---------------------------------------Set-----------------------------------------------

    v8::Local<v8::FunctionTemplate> FScriptSetWrapper::ToFunctionTemplate(v8::Isolate* Isolate)
    {
        v8::Isolate::Scope Isolatescope(Isolate);
        v8::EscapableHandleScope HandleScope(Isolate);
        auto Result = v8::FunctionTemplate::New(Isolate, New);
        Result->InstanceTemplate()->SetInternalFieldCount(4);//0 Ptr, 1 Property

        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Num"), v8::FunctionTemplate::New(Isolate, Num));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Add"), v8::FunctionTemplate::New(Isolate, Add));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Get"), v8::FunctionTemplate::New(Isolate, Get));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Contains"), v8::FunctionTemplate::New(Isolate, Contains));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "FindIndex"), v8::FunctionTemplate::New(Isolate, FindIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "RemoveAt"), v8::FunctionTemplate::New(Isolate, RemoveAt));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "GetMaxIndex"), v8::FunctionTemplate::New(Isolate, GetMaxIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "IsValidIndex"), v8::FunctionTemplate::New(Isolate, IsValidIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Empty"), v8::FunctionTemplate::New(Isolate, Empty));

        return HandleScope.Escape(Result);
    }

    void FScriptSetWrapper::Add(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        auto Property = Inner->Property;

        void* DataPtr = FMemory_Alloca(Property->GetSize());
        Property->InitializeValue(DataPtr);
        Inner->JsToUE(Isolate, Context, Info[0], DataPtr, false);

        auto ScriptLayout = FScriptSet::GetScriptLayout(Property->GetSize(), Property->GetMinAlignment());
        Self->Add(DataPtr, ScriptLayout,
            [Property](const void* Element) { return Property->GetValueTypeHash(Element); },
            [Property](const void* A, const void* B) { return Property->Identical(A, B); },
            [Property, DataPtr](void* Element)
            {
                Property->InitializeValue(Element);
                Property->CopySingleValue(Element, DataPtr);
            },
            [Property](void* Element) { Property->DestroyValue(Element); }
        );
        Property->DestroyValue(DataPtr);
    }

    void FScriptSetWrapper::Get(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        auto Property = Inner->Property;

        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        if (Self->IsValidIndex(Index) == false)
        {
            FV8Utils::ThrowException(Isolate, "invalid index argument");
        }
        else
        {
            auto ScriptLayout = FScriptSet::GetScriptLayout(Property->GetSize(), Property->GetMinAlignment());
            void* Data = Self->GetData(Index, ScriptLayout);
            Info.GetReturnValue().Set(Inner->UEToJs(Isolate, Context, Data, true));
        }
    }

    void FScriptSetWrapper::Contains(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        CHECK_V8_ARGS_LEN(1);
        bool Result = FindIndexInner(Info) != INDEX_NONE;
        Info.GetReturnValue().Set(Result);
    }

    void FScriptSetWrapper::FindIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        CHECK_V8_ARGS_LEN(1);
        int32 Result = FindIndexInner(Info);
        Info.GetReturnValue().Set(Result);
    }

    void FScriptSetWrapper::RemoveAt(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        auto Property = Inner->Property;

        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        if (Self->IsValidIndex(Index) == false)
        {
            FV8Utils::ThrowException(Isolate, "invalid index argument");
        }
        else
        {
            auto ScriptLayout = FScriptSet::GetScriptLayout(Property->GetSize(), Property->GetMinAlignment());
            void* Data = Self->GetData(Index, ScriptLayout);
            Destruct(Self, Inner, Index, 1);
            Self->RemoveAt(Index, ScriptLayout);
        }
    }

    void FScriptSetWrapper::GetMaxIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        uint32 Result = Self->GetMaxIndex();
        Info.GetReturnValue().Set(Result);
    }

    void FScriptSetWrapper::IsValidIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        uint32 Result = Self->IsValidIndex(Index);
        Info.GetReturnValue().Set(Result);
    }

    void FScriptSetWrapper::Empty(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        auto Property = Inner->Property;

        auto ScriptLayout = FScriptSet::GetScriptLayout(Property->GetSize(), Property->GetMinAlignment());
        Destruct(Self, Inner, 0, Self->Num());
        Self->Empty(0, ScriptLayout);
    }

    void FScriptSetWrapper::Destruct(FScriptSet *ScriptSet, FPropertyTranslator *Inner, int32 Index, int32 Count)
    {
        int32 MaxIndex = ScriptSet->GetMaxIndex();
        auto Property = Inner->Property;
        auto SetLayout = ScriptSet->GetScriptLayout(Property->GetSize(), Property->GetMinAlignment());
        for (int32 i = Index; i < Index + Count; ++i)
        {
            if (ScriptSet->IsValidIndex(i))
            {
                void* Data = ScriptSet->GetData(i, SetLayout);
                Property->DestroyValue(Data);
            }
        }
    }

    int32 FScriptSetWrapper::FindIndexInner(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptSet>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return INDEX_NONE;
        }
        auto Property = Inner->Property;

        void* DataPtr = FMemory_Alloca(Property->GetSize());
        Property->InitializeValue(DataPtr);

        Inner->JsToUE(Isolate, Context, Info[0], DataPtr, false);

        auto ScriptLayout = FScriptSet::GetScriptLayout(Property->GetSize(), Property->GetMinAlignment());

        int32 Result = Self->FindIndex(DataPtr, ScriptLayout,
            [Property](const void* Element) { return Property->GetValueTypeHash(Element); },
            [Property](const void* A, const void* B) { return Property->Identical(A, B); }
        );
        Property->DestroyValue(DataPtr);
        return Result;
    }

    //---------------------------------------Map-----------------------------------------------
    v8::Local<v8::FunctionTemplate> FScriptMapWrapper::ToFunctionTemplate(v8::Isolate* Isolate)
    {
        v8::Isolate::Scope Isolatescope(Isolate);
        v8::EscapableHandleScope HandleScope(Isolate);
        auto Result = v8::FunctionTemplate::New(Isolate, New);
        Result->InstanceTemplate()->SetInternalFieldCount(6);//0 Ptr, 1-2 Property

        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Num"), v8::FunctionTemplate::New(Isolate, Num));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Add"), v8::FunctionTemplate::New(Isolate, Add));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Get"), v8::FunctionTemplate::New(Isolate, Get));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Set"), v8::FunctionTemplate::New(Isolate, Set));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Remove"), v8::FunctionTemplate::New(Isolate, Remove));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "GetMaxIndex"), v8::FunctionTemplate::New(Isolate, GetMaxIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "IsValidIndex"), v8::FunctionTemplate::New(Isolate, IsValidIndex));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "GetKey"), v8::FunctionTemplate::New(Isolate, GetKey));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Empty"), v8::FunctionTemplate::New(Isolate, Empty));

        return HandleScope.Escape(Result);
    }

    void FScriptMapWrapper::Add(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(2);

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        auto KeyPropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        auto KeyProperty = KeyPropertyTranslator->Property;
        auto ValuePropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 2);
        auto ValueProperty = ValuePropertyTranslator->Property;
        if (!KeyPropertyTranslator->PropertyWeakPtr.IsValid() || !ValuePropertyTranslator->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "key/value info is invalid!");
            return;
        }

        void* KeyPtr = FMemory_Alloca(KeyProperty->GetSize());
        KeyProperty->InitializeValue(KeyPtr);

        void* ValuePtr = FMemory_Alloca(ValueProperty->GetSize());
        ValueProperty->InitializeValue(ValuePtr);

        KeyPropertyTranslator->JsToUE(Isolate, Context, Info[0], KeyPtr, false);
        ValuePropertyTranslator->JsToUE(Isolate, Context, Info[1], ValuePtr, false);

        auto ScriptLayout = FScriptMap::GetScriptLayout(KeyProperty->GetSize(), KeyProperty->GetMinAlignment(), ValueProperty->GetSize(), ValueProperty->GetMinAlignment());

        Self->Add(KeyPtr, ValuePtr, ScriptLayout,
            [KeyProperty](const void* ElementKey) { return KeyProperty->GetValueTypeHash(ElementKey); },
            [KeyProperty](const void* A, const void* B) { return KeyProperty->Identical(A, B); },
            [KeyProperty, KeyPtr](void* NewElementKey)
            {
                KeyProperty->InitializeValue(NewElementKey);
                KeyProperty->CopySingleValue(NewElementKey, KeyPtr);
            },
            [ValueProperty, ValuePtr](void* NewElementValue)
            {
                ValueProperty->InitializeValue(NewElementValue);
                ValueProperty->CopySingleValue(NewElementValue, ValuePtr);
            },
            [ValueProperty, ValuePtr](void* ExistingElementValue)
            {
                ValueProperty->CopySingleValue(ExistingElementValue, ValuePtr);
            },
            [KeyProperty](void* ElementKey)
            {
                KeyProperty->DestroyValue(ElementKey);
            },
            [ValueProperty](void* ElementValue)
            {
                ValueProperty->DestroyValue(ElementValue);
            }
        );
        KeyProperty->DestroyValue(KeyPtr);
        ValueProperty->DestroyValue(ValuePtr);
    }

    void FScriptMapWrapper::Get(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        auto KeyPropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        auto KeyProperty = KeyPropertyTranslator->Property;
        auto ValuePropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 2);
        auto ValueProperty = ValuePropertyTranslator->Property;
        if (!KeyPropertyTranslator->PropertyWeakPtr.IsValid() || !ValuePropertyTranslator->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "key/value info is invalid!");
            return;
        }

        void* KeyPtr = FMemory_Alloca(KeyProperty->GetSize());
        KeyProperty->InitializeValue(KeyPtr);
        KeyPropertyTranslator->JsToUE(Isolate, Context, Info[0], KeyPtr, false);
        
        auto ScriptLayout = FScriptMap::GetScriptLayout(KeyProperty->GetSize(), KeyProperty->GetMinAlignment(), ValueProperty->GetSize(), ValueProperty->GetMinAlignment());

        void* ValuePtr = Self->FindValue(KeyPtr, ScriptLayout,
            [KeyProperty](const void* ElementKey) { return KeyProperty->GetValueTypeHash(ElementKey); },
            [KeyProperty](const void* A, const void* B) { return KeyProperty->Identical(A, B); }
        );

        if (ValuePtr)
        {
            Info.GetReturnValue().Set(ValuePropertyTranslator->UEToJs(Isolate, Context, ValuePtr, true));
        }
        KeyProperty->DestroyValue(KeyPtr);
    }

    void FScriptMapWrapper::Set(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        Add(Info);
    }

    void FScriptMapWrapper::Remove(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        auto KeyPropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        auto KeyProperty = KeyPropertyTranslator->Property;
        auto ValuePropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 2);
        auto ValueProperty = ValuePropertyTranslator->Property;
        if (!KeyPropertyTranslator->PropertyWeakPtr.IsValid() || !ValuePropertyTranslator->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "key/value info is invalid!");
            return;
        }

        void* KeyPtr = FMemory_Alloca(KeyProperty->GetSize());
        KeyProperty->InitializeValue(KeyPtr);
        KeyPropertyTranslator->JsToUE(Isolate, Context, Info[0], KeyPtr, false);

        auto ScriptLayout = GetScriptLayout(KeyProperty, ValueProperty);
        int32 Index = Self->FindPairIndex(KeyPtr, ScriptLayout,
            [KeyProperty](const void* Key) { return KeyProperty->GetValueTypeHash(Key); },
            [KeyProperty](const void* A, const void* B) { return KeyProperty->Identical(A, B); }
        );
        if (Index == INDEX_NONE)
        {
            FV8Utils::ThrowException(Isolate, TEXT("invalid key argument"));
        }
        else
        {
            Destruct(Self, KeyPropertyTranslator, ValuePropertyTranslator, Index, 1);
            Self->RemoveAt(Index, ScriptLayout);
        }
        KeyProperty->DestroyValue(KeyPtr);
    }

    void FScriptMapWrapper::GetMaxIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        int32 Result = Self->GetMaxIndex();
        Info.GetReturnValue().Set(Result);
    }

    void FScriptMapWrapper::IsValidIndex(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        Info.GetReturnValue().Set(Self->IsValidIndex(Index));
    }

    void FScriptMapWrapper::GetKey(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        CHECK_V8_ARGS_LEN(1);

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        int32 Index = Info[0]->Int32Value(Context).ToChecked();
        if (Self->IsValidIndex(Index) == false)
        {
            FV8Utils::ThrowException(Isolate, TEXT("the argument is an invalid index"));
            Info.GetReturnValue().Set(v8::Undefined(Isolate));
        }
        else
        {
            auto KeyPropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
            auto KeyProperty = KeyPropertyTranslator->Property;
            auto ValuePropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 2);
            auto ValueProperty = ValuePropertyTranslator->Property;
            if (!KeyPropertyTranslator->PropertyWeakPtr.IsValid() || !ValuePropertyTranslator->PropertyWeakPtr.IsValid())
            {
                FV8Utils::ThrowException(Isolate, "key/value info is invalid!");
                return;
            }
            
            auto ScriptLayout = GetScriptLayout(KeyProperty, ValueProperty);
            uint8* Data = reinterpret_cast<uint8*>(Self->GetData(Index, ScriptLayout));
            void* KeyPtr = Data + GetKeyOffset(ScriptLayout);
            Info.GetReturnValue().Set(KeyPropertyTranslator->UEToJs(Isolate, Context, KeyPtr, true));
        }
    }
    
    void FScriptMapWrapper::Empty(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<FScriptMap>(Info.Holder(), 0);
        auto KeyPropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        auto KeyProperty = KeyPropertyTranslator->Property;
        auto ValuePropertyTranslator = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 2);
        auto ValueProperty = ValuePropertyTranslator->Property;
        if (!KeyPropertyTranslator->PropertyWeakPtr.IsValid() || !ValuePropertyTranslator->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "key/value info is invalid!");
            return;
        }
        
        auto ScriptLayout = GetScriptLayout(KeyProperty, ValueProperty);
        Destruct(Self, KeyPropertyTranslator, ValuePropertyTranslator, 0, Self->Num());
        Self->Empty(0, ScriptLayout);
    }

    void FScriptMapWrapper::Destruct(FScriptMap *ScriptMap, FPropertyTranslator *KeyTranslator, FPropertyTranslator *ValueTranslator, int32 Index, int32 Count)
    {
        int32 MaxIndex = ScriptMap->GetMaxIndex();
        auto KeyProperty = KeyTranslator->Property;
        auto ValueProperty = ValueTranslator->Property;
        auto MapLayout = GetScriptLayout(KeyProperty, ValueProperty);
        for (int32 i = Index; i < Index + Count; ++i)
        {
            if (ScriptMap->IsValidIndex(i))
            {
                uint8* Data = reinterpret_cast<uint8*>(ScriptMap->GetData(i, MapLayout));
                void* Key = Data + GetKeyOffset(MapLayout);
                void* Value = Data + MapLayout.ValueOffset;
                KeyProperty->DestroyValue(Key);
                ValueProperty->DestroyValue(Value);
            }
        }
    }

    FScriptMapLayout FScriptMapWrapper::GetScriptLayout(const PropertyMacro* KeyProperty, const PropertyMacro* ValueProperty)
    {
        return FScriptMap::GetScriptLayout(KeyProperty->GetSize(), KeyProperty->GetMinAlignment(), ValueProperty->GetSize(), ValueProperty->GetMinAlignment());
    }

    //---------------------------------------Fix Size Array-----------------------------------------------
    v8::Local<v8::FunctionTemplate> FFixSizeArrayWrapper::ToFunctionTemplate(v8::Isolate* Isolate)
    {
        v8::Isolate::Scope Isolatescope(Isolate);
        v8::EscapableHandleScope HandleScope(Isolate);
        auto Result = v8::FunctionTemplate::New(Isolate, New);
        Result->InstanceTemplate()->SetInternalFieldCount(4);//0 Ptr, 1 Property

        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Num"), v8::FunctionTemplate::New(Isolate, Num));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Get"), v8::FunctionTemplate::New(Isolate, Get));
        Result->PrototypeTemplate()->Set(FV8Utils::InternalString(Isolate, "Set"), v8::FunctionTemplate::New(Isolate, Set));
        //Result->PrototypeTemplate()->SetIndexedPropertyHandler(Getter, Setter);

        return HandleScope.Escape(Result);
    }

    void FFixSizeArrayWrapper::Num(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        Info.GetReturnValue().Set(Inner->Property->ArrayDim);
    }

    void FFixSizeArrayWrapper::Get(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<uint8>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        auto Property = Inner->Property;

        int32 Index = -1;

        if (Info[0]->IsInt32())
        {
            Index = Info[0]->Int32Value(Context).ToChecked();
        }

        if (Index < 0 || Index >= Property->ArrayDim)
        {
            FV8Utils::ThrowException(Isolate, "invalid index");
            return;
        }

        auto Ptr = Self + Property->ElementSize * Index;

        Info.GetReturnValue().Set(Inner->UEToJs(Isolate, Context, Ptr, true));
    }

    void FFixSizeArrayWrapper::Set(const v8::FunctionCallbackInfo<v8::Value>& Info)
    {
        v8::Isolate* Isolate = Info.GetIsolate();
        v8::HandleScope HandleScope(Isolate);
        v8::Local<v8::Context> Context = Isolate->GetCurrentContext();

        auto Self = FV8Utils::GetPointerFast<uint8>(Info.Holder(), 0);
        auto Inner = FV8Utils::GetPointerFast<FPropertyTranslator>(Info.Holder(), 1);
        if (!Inner->PropertyWeakPtr.IsValid())
        {
            FV8Utils::ThrowException(Isolate, "item info is invalid!");
            return;
        }
        auto Property = Inner->Property;

        int32 Index = -1;

        if (Info[0]->IsInt32())
        {
            Index = Info[0]->Int32Value(Context).ToChecked();
        }

        if (Index < 0 || Index >= Property->ArrayDim)
        {
            FV8Utils::ThrowException(Isolate, "invalid index");
            return;
        }

        auto Ptr = Self + Property->ElementSize * Index;

        Inner->JsToUE(Isolate, Context, Info[1], Ptr, true);
    }
}
