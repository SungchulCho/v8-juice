// EXPERIMENTAL/INCOMPLETE!
#include <sstream>

#include "ClassWrap.h"


#include <unistd.h> // sleep(3)

#ifndef CERR
#include <iostream> /* only for debuggering */
#define CERR std::cerr << __FILE__ << ":" << std::dec << __LINE__ << " : "
#endif

namespace v8 { namespace juice {
    /** A class for testing ClasWrap. */
    struct BoundNative
    {
    public:
        static bool enableDebug;
    private:
        static size_t instcount;
#define DBGOUT if(BoundNative::enableDebug) CERR
        int propi;
    public:
        double publicProperty;
        //int publicProperty;
        BoundNative()
            : propi(13),
              publicProperty(42.24)
        {
            ++instcount;
        }
        ~BoundNative()
        {
            --instcount;
        }
        bool ptr( BoundNative const * b )
        {
            DBGOUT << "BoundNative[@"<<(void const *)this<<"]->ptr("<<(void const *)b<<")\n";
            return 0 != b;
        }
        BoundNative * getPtr()
        {
            return this;
        }
        static size_t InstanceCount()
        {
            return instcount;
        }
        int getInt() const { return this->propi; }
        void setInt( int i ) { this->propi = i; }
        std::string toString() const
        {
            std::ostringstream os;
            os << "[Object BoundNative@"<<(void const *)this<<']';
            return os.str();
        }
        void tryRoundaboutApproach()
        {
            DBGOUT << "BoundNative[@"<<(void const *)this<<"]::tryRoundaboutApproach()\n";
        }
        Handle<Value> toString2( Arguments const & argv )
        {
#if 0
            typedef convert::MemFuncForwarder<0> MF;
            return MF::Call( this, &BoundNative::tryRoundaboutApproach, argv );
#else
            return convert::CastToJS( this->toString() );
#endif
        }
    };
    size_t BoundNative::instcount = 0;
    bool BoundNative::enableDebug = true;

    template <>
    struct ClassWrap_ToNative_SearchPrototypesForNative<BoundNative>
        : ClassWrap_Opt_Bool<true>
    {};

#if 1
    // Only used for testing some compile-time assertions:
    template <>
    struct ClassWrap_InternalFields<BoundNative> : ClassWrap_Opt_Int<4>
    {
        static const int NativeIndex = 2;
    };
#endif

#if 1
    template <>
    struct ClassWrap_Inheritance<BoundNative>
        : ClassWrap_Inheritance_Base<
        BoundNative
        //,std::string // should fail to compile
        //,BoundNative // should work
        >
    {};
#endif

#if 0
    //#  warning "Using JuiceBind policies!"
#  define USING_JUICEBIND_POLICIES
#elif 0
    //#  warning "Using Experimental policies!"
#  define USING_TWOWAY_POLICIES
#else
    //#  warning "Using default policies!"
#  define USING_DEFAULT_POLICIES
#endif

} } // nemspaces

#define CLASSWRAP_BOUND_TYPE v8::juice::BoundNative
#define CLASSWRAP_BOUND_TYPE_NAME "BoundNative"
#if defined(USING_TWOWAY_POLICIES)
#  include "ClassWrap_TwoWay.h"
#elif defined(USING_JUICEBIND_POLICIES)
#  include "ClassWrap_JuiceBind.h"
#elif defined(USING_DEFAULT_POLICIES)
#  include "ClassWrap-JSToNative.h"
#  undef CLASSWRAP_BOUND_TYPE_NAME
#endif

namespace v8 { namespace juice {

#if defined(USING_DEFAULT_POLICIES)
    template <>
    struct ClassWrap_ClassName<BoundNative>
    {
        static char const * Value()
        {
            return "BoundNative";
        }
    };
#endif

    
    template <>
    struct ClassWrap_Factory<BoundNative>
    {
        typedef convert::TypeInfo<BoundNative>::Type Type;
        typedef convert::TypeInfo<BoundNative>::NativeHandle NativeHandle;
	static NativeHandle Instantiate( Arguments const &  /*argv*/,
                                         std::ostream & /* exceptionText */ )
	{
            NativeHandle x = new BoundNative;
            DBGOUT << "BoundNative->Instantiate() == @"<<(void const *)x<<'\n';
            return x;
	}
	static void Destruct( NativeHandle obj )
	{
            DBGOUT << "BoundNative->Destruct() == @"<<(void const *)obj<<'\n';
#if defined(USING_TWOWAY_POLICIES)
            typedef Detail::ClassWrapMapper<BoundNative> Mapper;
            Mapper::Remove( obj );
#elif defined(USING_JUICEBIND_POLICIES)
            bind::UnbindNative( obj );
#endif
            delete obj;
	}
        static const size_t AllocatedMemoryCost = sizeof(BoundNative);
    };
}} // namespaces


namespace v8 { namespace juice {
    std::string BoundNative_version()
    {
        return "alphalpha 0.0.1";
    }
    void BoundNative_doSomething(std::string const &x)
    {
        DBGOUT << "doSomething(\""<<x<<"\")\n";
    }
    size_t BoundNative_doSomething2(std::string const &x)
    {
        DBGOUT << "doSomething2(\""<<x<<"\")\n";
        return 42;
    }

    v8::Handle<v8::Value> BoundNative_destroy( v8::Arguments const & argv )
    {
        DBGOUT << "BoundNative_Destroy()\n";
        return convert::CastToJS( ClassWrap<BoundNative>::DestroyObject(argv.This()) );
    }

    void bind_my_native( v8::Handle<v8::Object> dest )
    {
        typedef ClassWrap_Inheritance<BoundNative> Inherit;
        {
            Inherit x;
        }
        using namespace v8;
        using namespace v8::juice;
        HandleScope scope;
        typedef BoundNative N;
        typedef ClassWrap<N> CW;
        CW & cw( CW::Instance() );
        DBGOUT <<"Binding class "<<CW::ClassName::Value()<<"...\n";
        cw.Set("foo",String::New("this is foo"));
        cw.Set("toString2", convert::InvocationCallbackMember<N,&N::toString2>::Call );
        //cw.BindMemberFunc<&N::toString2>("toString");
        typedef
            convert::InvocationCallbackCreator
            //convert::FunctionForwarder<0>
            ICC;
        cw.Set( "toString",
                ICC::M0::Invocable<N,std::string,&N::toString>
                );
        cw.Set( "getInt",
                ICC::M0::Invocable<N,int,&N::getInt>
                );
        cw.Set( "setInt",
                ICC::M1::Invocable<N,void,int,&N::setInt>
                );
        cw.Set( "ptr",
                ICC::M1::Invocable<N,bool,N const * ,&N::ptr>
                //ICC::M1::InvocableVoid<N,bool,N const * ,&N::ptr>
                );
#if defined(USING_TWOWAY_POLICIES)
        cw.Set( "getPtr",
                ICC::M0::Invocable<N,N*,&N::getPtr>
                //ICC::M1::InvocableVoid<N,bool,N const * ,&N::ptr>
                );
#endif
        //typedef convert::PropertyBinder<N> PB;
        typedef CW::PB PB;
        v8::Handle<v8::ObjectTemplate> cwproto = cw.Prototype();
        PB::BindGetterSetter<int,&N::getInt,void,int,&N::setInt>( "myInt", cwproto );
        PB::BindGetter<int,&N::getInt>( "intGetter", cwproto );
        PB::BindMemVar<double,&N::publicProperty>( "publicProperty", cwproto );
        PB::BindMemVarRO<double,&N::publicProperty>( "publicPropertyRO", cwproto );

        PB::BindStaticVar<bool,&N::enableDebug>( "debug", cwproto );
        PB::BindStaticVarRO<bool,&N::enableDebug>( "debugRO", cwproto );
        
        v8::InvocationCallback FH;
        FH =
            ICC::F0::Invocable<std::string,BoundNative_version>
            //ICC::Invocable<std::string,BoundNative_version>
            ;
#define JFH v8::FunctionTemplate::New(FH)->GetFunction()
        cw.Set( "version", JFH );

        FH = ICC::F1::Invocable<void,std::string const &,BoundNative_doSomething>;
        FH = ICC::F1::InvocableVoid<size_t,std::string const &,BoundNative_doSomething2>;
        FH = ICC::F1::Invocable<size_t,std::string const &,BoundNative_doSomething2>;
        cw.Set( "doSomething", JFH );

        cw.Set( "destroy", CW::DestroyObject );
        //cw.Set( "destroy", BoundNative_destroy );

        FH = ICC::F0::Invocable<size_t,BoundNative::InstanceCount>;
        cw.Set( "instanceCount", JFH );
        cw.CtorTemplate()->Set( "instanceCount", JFH );
        cw.CtorTemplate()->Set( "supportsInheritance",
                                convert::CastToJS(ClassWrap_ToNative_SearchPrototypesForNative<BoundNative>::Value) );
        FH = ICC::F1::Invocable<unsigned int,unsigned int,::sleep>;
        cw.Set( "sleep", JFH );
        v8::Handle<v8::Function> ctor = cw.Seal();
        cw.AddClassTo( dest );
//         PB::BindStaticVar<bool,&N::enableDebug>( "debug", ctor );
#undef JFH

        //v8::HandleScope hscope;
        Handle<Object> jobj =
            cw.NewInstance(0,0)
            //ctor->NewInstance(0,0)
            ;
        N * bound = CW::ToNative::Value(jobj);
        DBGOUT << "bound (void *) == @"<<(void const *)bound<<'\n';
        DBGOUT << "bound (CastFromJS<T>(jsObj)) == @"<<convert::CastFromJS<N>( jobj )<<'\n';
        if( bound )
        {
            typedef convert::MemFuncForwarder<0> MFF;
            //Handle<Value> = MFF::Call( obj, BoundNative::toString
        }
        if(1)
        {
            v8::HandleScope hs;
            int level = 1;
            v8::Local<v8::Value> proto = jobj->GetPrototype();
            for( ; ! proto.IsEmpty() && proto->IsObject(); ++level )
            {
                DBGOUT << "Prototype level "<<level<<'\n';
                proto = Local<Object>( v8::Object::Cast(*proto) )->GetPrototype();
            }
        }

        if(1)
        {
            v8::Handle<v8::Object> so = ClassWrap_FindHolder<N>( jobj, bound );
            DBGOUT << "FindHolder() == [" << convert::CastFromJS<std::string>( so ) << "]\n";
        }

        DBGOUT << "BoundNative::InstanceCount() == "<<BoundNative::InstanceCount()<<'\n';
        CW::DestroyObject(jobj);
        DBGOUT << "BoundNative::InstanceCount() == "<<BoundNative::InstanceCount()<<'\n';
#if 0
        if(0 && bound)
        {
            Handle<Object> j2 = CW::CastToJS::Value( bound );
            DBGOUT << "JW::CastToJS::Value(jobj) == "<<convert::CastFromJS<std::string>(j2)<<'\n';
        }
#endif
        DBGOUT <<"Binding done.\n";
    }

} } // namespaces

