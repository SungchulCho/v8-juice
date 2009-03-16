#ifndef s11n_net_sq3_refcount_REFCOUNT_HPP_INCLUDED
#define s11n_net_sq3_refcount_REFCOUNT_HPP_INCLUDED 1
// reminders to self:
// - think about lazyassptr<T> which lazily instantiates its
// pointee. That would take a Constructor functor, providing symmetry
// with rcptr<>.

#include <map>
////////////////////////////////////////////////////////////////////////
//    This code encapsulates a reference-counted smart pointer. It is
//    capable of tracking and destroying objects and arbitrary
//    pointers (including void pointers) and destroying them using a
//    user-defined finalizer functor. This allows, e.g., the
//    reference-counted sharing of memory allocated via malloc() or by
//    third-party functions such as dlopen() or sqlite3_open().
//
//    This code is not generic, industrial-strength reference counting
//    and is as much an experiment as anything else.
//
//    Author: stephan at s11n dot net
//
//    License: Public Domain
////////////////////////////////////////////////////////////////////////

#ifndef RCPTR_DEBUG
#  define RCPTR_DEBUG 0
#endif

#if RCPTR_DEBUG
#  include <iostream>
#  define RCOUT if( RCPTR_DEBUG ) std::cerr << __FILE__ << ":" << std::dec<<__LINE__ << ": " << "[@"<<std::hex << this->m_ptr << "] "
#  define RCOUT2(PTR) if( RCPTR_DEBUG ) std::cerr << __FILE__ << ":" << std::dec<<__LINE__ << ": " << "[ptr@"<<std::hex << PTR << "] "
#endif // RCPTR_DEBUG

namespace sq3 {


    /**
       A no-op "destructor" for use with rcptr.
    */
    struct no_delete_finalizer
    {
        /** Assigs t to 0 without deleting t. */
        template <typename T>
        void operator()( T * & t )
        {
            t = 0;
        }
    };

    /**
       The default destructor/cleanup functor for use with
       rcptr<>.
    */
    struct plain_delete_finalizer
    {
        /**
           Calls delete t and assigns t to 0.
			   
           Specialized dtors need not call delete, but should
           assign t to 0, as this simplifies some client code.

           T must be non-CVP-qualified and for this
           implementation (delete t) must be legal.
        */
        template <typename T>
        void operator()( T * & t )
        {
#if RCPTR_DEBUG
            RCOUT2(t) << "plain_delete_finalizer::operator(@"<<std::hex<<t<<")\n";
#endif
            delete t;
            t = 0;
        }
    };

    /**
       All classes in this namespace are "internal details" of the
       classes in the refcount namespace, and should not be
       directly used by client code.
    */
    namespace Detail
    {
        /**
           Internal detail for dereferencing pointers.
        */
        template <typename T>
        struct ref_type
        {
            /** Same as (T&). */
            typedef T & type;
            /** Returns *t. */
            static type deref( T *t ) { return *t; }
        };
        /**
           Internal detail for dereferencing pointers.
        */
        template <>
        struct ref_type<void>
        {
            /** Same as (void*&). */
            typedef void * & type;
            /** Returns xx. */
            static type deref( type xx ) { return xx; }
        };
    } // namespace Detail

    /**
       A bare-bones non-intrusive reference-counted pointer type
       with the ability for the client to specify a
       finalization/destruction functor for the pointed-to type.

       HandleT must be a non-CVP-qualified type. As a special
       case, if HandleT is void then some code in this class will
       work a bit differently, notably the operator*(), because we
       cannot form a reference to void. Void is supported because
       (void *) is commonly used for opaque handles (e.g. libdl)
       or multibyte string pointers (e.g. libsqlite3).

       FinalizerT must be a type compatible with the
       plain_delete_finalizer interface. A default-constructed
       instance of that FinalizerT type will be created to
       "finalize" an object when the reference count for that
       object drops to zero. The exact behaviour of the FinalizerT
       is not specified here, but semantically it must "finalize"
       the object passed to it. The default finalizer simply
       deletes the object, whereas a more advanced finalizer might
       push the object into a garbage collection pool. For
       purposes of this class, after finalization of an object,
       client code (and this type) should no longer use the object
       - it is considered to be destroyed.

       This type does not currently have any built-in support for
       copy-on-write, so all copies are extremely shallow.

       Notes of Utmost Significance to Potential Users:

       - Implicit conversions to/from HandleT are not implemented
       after much deliberation on the subject. Clients will *have*
       to know they're using this class, as opposed to a plain
       pointer type. This is safest for everyone, IMO.

       - Don't mix plain and rcptr-hosted pointers, as the rcptr
       wrappers own the pointers and will clean them up, leaving
       any unadorned pointers dangling.

       - Thread safety: no special guarantees, along with lots of
       caveats and potential gotchas.

       - Don't mix different smart pointer types, not even
       rcptrs with the same HandleT type but different
       FinalizerT types. This will almost certainly bring about
       the incorrect finalization of a pointer.

       - The usage of a finalizer functor means that this type can
       be used with arbitrary types, regardless of whether the
       delete operation is legal or not on them. For example, the
       client code for which this class was written uses a functor
       to finalize sqlite3 database handles using the
       sqlite3_close() function.
	   


       Design notes:

       - While originally based off of the presentation of
       rc-pointers in Meyers' "More Effective C++", Item 29, i
       believe his approach to storing the reference count in his
       RCIPtr class is flawed, as it allows multiple rc-pointers
       to delete the same pointer. Consider:

       \code
       typedef RCIPtr<MyType> myPtrType;
       MyType * t = new MyType;
       myPtrType x(t);
       myPtrType y(t);
       \endcode

       In theory, his presentation (admittedly 10+ years old now)
       would cause a double-delete for that case. In this model,
       that case is handled as if we had constructed y using y(x)
       instead of y(t), so both x and y share the reference count.

       - The reference count is stored in a static-space std::map,
       and that map is specific to this type and its combination
       of HandleT/FinalizerT types. If we made the map only
       specific to the HandleT, then we would get
       strange/undesired behaviour when we did:

       \code
       rcptr<T1,finalizerT1> p1( new T1 );
       rcptr<T2,finalizerT2> p2( p1.get() );
       \endcode

       because the actual finalizer used would be the one for
       which the rcptr is destroyed *last*. Since destruction
       order is not always determinate, this mixture would be a
       bad idea. Note that it is still illegal to add the same
       pointer to multiple different shared pointer types. The
       above example, while illegal, will at least cause
       determinate behaviour: a double finalization (but the order
       is still unspecified in the general case)!


       Fundamental differences between rcptr and
       boost::shared_ptr:

       - rcptr::takeover() allows client to take ownership of a
       pointer away from rcptr. According to the shared_ptr FAQ,
       this isn't technically feasible in that class due to their
       handling of the user-defined finalizer.

       - rcptr has no explicit support for multi-threading.

       - shared_ptr does not handle the following code "correctly"
       (IMO):

       typedef boost::shared_ptr<AStruct> SP;
       SP sp1( new AStruct );
       SP sp2( sp1.get() );
       // sp1.use_count() is 1, not 2
       // This causes a double deletion

       rcptr handles that case transparently.

    */
    template <typename HandleT,
              typename FinalizerT = plain_delete_finalizer>
    class rcptr
    {
    public:
        /**
           The basic type of object pointed to.
        */
        typedef HandleT type;
        /**
           The basic pointer type.
        */
        typedef type * pointer_type;
        /** The type of functor used to clean up pointer_type objects. */
        typedef FinalizerT finalizer_type;
    private:
        mutable pointer_type m_ptr;
        typedef int counter_type;
        typedef std::map<pointer_type,counter_type> map_type;
        /** Returns a shared map holding the reference
            counts for all instances of pointer_type
            tracked by this class. This is not
            post-main() safe.
        */
        static map_type & map()
        {
            static map_type bob;
            return bob;
        }

        /** 
            Decrements the reference count to ptr.  If the
            count goes to 0, an instance of finalizer_type is
            used to "destruct" ptr.  On success, the current
            reference count is returned.  If 0 is returned,
            ptr should be considered invalid (though this
            actually depends on finalizer_type's
            implementation, the semantics are that destruction
            leaves us with an unusable object).  On error
            (passed a null ptr), a number less than 0 is
            returned.
        */
        static counter_type decrement( pointer_type & ptr )
        {
#if RCPTR_DEBUG
            RCOUT2(ptr) << "decrement([@"<<std::hex<<ptr<<"])\n";
#endif
            if( ! ptr ) return -1;
            map_type & m = map();
            typename map_type::iterator it = m.find(ptr);
            if( m.end() == it ) return 0;
#if 0
            if( 0 == (*it).second )
            {
                // can happen???
                m.erase(it);
                return 0;
            }
#endif // 0|1
            counter_type rc = --(*it).second;
#if RCPTR_DEBUG
            RCOUT2(ptr) << "decrement([@"<<std::hex<<ptr<<"]) rc=="<<rc<<'\n';
#endif
            //CERR << "rcptr<>::--("<<std::hex<<ptr<<"): rc=="<<(*it).second<<'\n';
            if ( 0 == rc )
            {
#if RCPTR_DEBUG
                RCOUT2(ptr) << "decrement([@"<<std::hex<<ptr<<"]) finalizing @"<<ptr<<"\n";
                // ??? causing a double delete sometimes???
                // m.erase( it );
                RCOUT2(ptr) << "decrement([@"<<std::hex<<ptr<<"]) really finalizing @"<<ptr<<"\n";
#endif
                finalizer_type()( ptr );
                /***
                    BUG: due to a crash in the STL routines (or lower level),
                    we cannot properly clean up the map when rc==0 (see above).

                    One (very pitiful) option is to
                    add a loop here to try to work
                    around the problem until the reason
                    for the low-level "duplicate delete" crash
                    can be determined.
                */
                //RCOUT2(ptr) << "decrement([@"<<std::hex<<ptr<<"]) cleanup kludge... @"<<ptr<<"\n";
                for( it = m.begin(); m.end() != it; ++it )
                {
                    if( (*it).second <= 0 ) m.erase(it);
                }
            }
            return rc;
        }

        /**
           If ! ptr, does nothing, else it increases the
           reference count for ptr by one. Returns the current
           reference count (guaranteed to be 1 or higher) on
           success, or a negative number if passed a null ptr.
        */
        static counter_type increment( pointer_type & ptr )
        {
            //RCOUT2(ptr) << "increment([@"<<std::hex<<ptr<<"])\n";
            if( ! ptr ) return -1;
#if ! RCPTR_DEBUG
            return ++(map()[ptr]);
#else
            counter_type c = ++(map()[ptr]);
            RCOUT2(ptr) << "increment([@"<<std::hex<<ptr<<"]) rc=="<<c<<'\n';
            return c;
#endif // 0|1
        }

        // 		bool safety_first()
        // 		{
        // 			if( ! this->m_ptr ) return false;
        // 			if( 0 == this->ref_count() )
        // 			{ // dangling pointer, it seems
        // 				this->m_ptr = 0;
        // 			}
        // 			return 0 != this->m_ptr;
        // 		}
    public:
        /**
           Transfers ownership of h, or allows h to
           participate in ownership with other rcptr
           objects pointing at h.
        */
        explicit rcptr( pointer_type h ) : m_ptr(h)
        {
            // 			RCOUT << "rcptr("<<std::hex<<h<<")\n";
            // 			if( this->m_ptr == h ) return;
#if RCPTR_DEBUG
            RCOUT << "rcptr("<<std::hex<<h<<")\n";
#endif
            this->increment( this->m_ptr );
        }
        /**
           rhs and this object will both manage the same
           underlying pointer.
        */
        rcptr( rcptr const & rhs ) : m_ptr(rhs.m_ptr)
        {
#if RCPTR_DEBUG
            RCOUT << "rcptr( rcptr const & ["<<std::hex<<this->m_ptr<<"])\n";
#endif
            this->increment( this->m_ptr );
        }
        /**
           Disowns any currently-connected pointer, then rhs
           and this object will both manage the same
           underlying pointer. If by chance rhs.get() ==
           this->get() when this function starts then this
           function does nothing and has no side effects.
        */
        rcptr & operator=( rcptr const & rhs )
        {
#if RCPTR_DEBUG
            RCOUT << "operator=( rcptr const & ["<<std::hex<<rhs.m_ptr<<"])\n";
#endif
            if( rhs.m_ptr == this->m_ptr ) return *this;
            this->take( rhs.m_ptr );
#if 0
            this->decrement( this->m_ptr );
            this->increment( this->m_ptr );
#elseif 0
            pointer_type orig = this->m_ptr;
            this->increment( this->m_ptr = rhs.m_ptr );
            this->decrement( orig );
#endif // 0|1
            return *this;
        }
        /**
           An empty shared pointer, useful only as a target of
           assigment or take().
        */
        rcptr() : m_ptr(0)
        {}

        /**
           Efficiently swaps this object and rhs, such that they
           swap ownership of their underlying pointers.
           This does not require fiddling with the reference
           counts, so it is much faster than using an rcptr
           copy to perform a swap.
        */
        void swap( rcptr & rhs )
        {
            if( this->m_ptr != rhs )
            {
                pointer_type x = this->m_ptr;
                this->m_ptr = rhs.m_ptr;
                rhs.m_ptr = x;
            }
        }


        /**
           See decrement();
        */
        ~rcptr()
        {
#if RCPTR_DEBUG
            RCOUT << "~rcptr()\n";
#endif
            //this->decrement( this->m_ptr );
            this->drop();
        }

        /** Returns (this->m_ptr == rhs.m_ptr). */
        bool operator==( rcptr const & rhs ) const
        {
            return this->m_ptr == rhs.m_ptr;
        }
        /** Returns (this->m_ptr != rhs.m_ptr). */
        bool operator!=( rcptr const & rhs ) const
        {
            return this->m_ptr != rhs.m_ptr;
        }

        /**
           Returns this->get() < rhs.get(). Implemented so that
           this type can be used as keys in STL containers.
        */
        bool operator<( rcptr const & rhs ) const
        {
            return this->m_ptr < rhs.m_ptr;
        }

        /** Returns this object's underlying pointer, which
            may be 0. This does not transfer ownership. This
            object still owns (or participates in the
            ownership of) the returned pointer.
        */
        pointer_type get() const { return this->m_ptr; }

        /**
           Gives ownership of p to this object (or a
           collection of like-typed rcptr objects). If
           (this->get() == p) then this function does nothing.
           Note that this->get() might get finalized as
           a result of this call.

           Returns p.
        */
        pointer_type take( pointer_type p )
        {
#if RCPTR_DEBUG
            RCOUT << "take( ["<<std::hex<<p<<"] )\n";
#endif
            if( p == this->m_ptr ) return p;
#if 1
            //RCOUT << "take( ["<<std::hex<<p<<"] ) decrementing...\n";
            this->decrement( this->m_ptr );
            //RCOUT << "take( ["<<std::hex<<p<<"] ) incrementing...\n";
            this->increment( this->m_ptr = p );
#else
            pointer_type orig = this->m_ptr;
            this->increment( this->m_ptr = p );
            this->decrement( orig );
#endif
#if RCPTR_DEBUG
            RCOUT << "take( ["<<std::hex<<p<<"] ) done.\n";
#endif
            return p;
        }

        /**
           Transfers ownership of this->get() to the caller.

           ALL rcptr<> objects of this type which point to that object
           (except this rcptr) STILL point to that object, but they
           will not activate the destructor functor when they die, so
           they are safe as long as they remain unsued or are
           destroyed before the "raw" pointer returned from this
           function is destroyed.
        */
        pointer_type takeover()
        {
            if( this->m_ptr )
            {
                pointer_type t = this->m_ptr;
                this->map().erase( this->m_ptr );
                this->m_ptr = 0;
                return t;
            }
            return this->m_ptr;
        }

        /**
           The same as this->get().
        */
        pointer_type operator->() const { return this->m_ptr; }


        /**
           reference_type is the same as (T&) unless T is void,
           in which case it is the same as (void*&) because
           (void&) is not legal.
        */
        typedef typename Detail::ref_type<type>::type reference_type;

        /**
           The same as *(this->get()). Behaviour is undefined
           if (!this->get()). We would throw an exception, but
           this code is specifically intended for use on
           platforms where exceptions are not allowed or not
           supported (e.g. some embedded platforms).

           SPECIAL CASE: rcptr::type is void

           If rcptr::type is void then this function returns a
           reference to a pointer instead of a reference. This
           is to allow this type to work with (void*) handle
           types, such as handles returned from dlopen() or
           memory returned from malloc(). Finalizers for such
           handles could call dlclose() or free(), as
           appropriate.
        */
        reference_type operator*() const
        {
            return Detail::ref_type<type>::deref( this->m_ptr );
        }
        /**
           Returns the number of references to this object's pointer,
           or zero if no pointer is bound. This function should be
           considered a debugging/informational function, and not
           a "feature" of this type.

           Complexity = that of a std::map lookup.
        */
        size_t ref_count() const
        {
            if( ! this->m_ptr ) return 0;
            typename map_type::iterator it = map().find(this->m_ptr);
            return ( map().end() == it )  ? 0 : (*it).second;
        }

        /**
           Decrements the refcount for this->get() (potentially
           causing its destruction) and sets this object's
           shared pointer to null. Further calls to this->get()
           will return 0 until this->take() is called.

           Returns false if this->empty(), else true.
        */
        bool drop()
        {
            if( ! this->m_ptr ) return false;
            this->decrement( this->m_ptr );
            this->m_ptr = 0;
            return true;
        }

        /**
           Returns the same as (!this->get()).
        */
        bool empty() const { return 0 == this->m_ptr; }

        // Adding deep copy support requires a copy ctor/functor for our
        // pointee type, but this type should/must be usable with opaque
        // pointer handles as well as object pointers (e.g. sqlite3 db handles
        // and ncurses WINDOW handles). But if you did want to implement
        // copy(), here's how you might go about doing it...
        // 		/**
        // 		   Makes a copy of p using (new type(*p)) and
        // 		   transfers ownership of that copy to this
        // 		   object. Further copies of this object will point to
        // 		   that copy unless/until copy() is called on
        // 		   them. This function is intended to simplify
        // 		   implementation of copy-on-write. If p is null then
        // 		   this object points to null.

        // 		   To force an rcptr to copy its current pointer, simply
        // 		   call ptr.copy( ptr.get() ).
        // 		*/
        // 		void copy( pointer_type p )
        // 		{
        // 			pointer_type x = this->m_ptr;
        // 			if( p )
        // 			{
        // 				this->m_ptr = new type(*p);
        // 				this->increment( this->m_ptr );
        // 			}
        // 			else
        // 			{
        // 				this->m_ptr = 0;
        // 			}
        // 			this->decrement(x);
        // 		}
        // Some things to consider:
        // 		bool m_shareable;
        // 		bool shareable() const { return this->m_shareable; }
        // 		void shareable( bool s ) { this->m_shareable = s; }
        // 		bool shared() const { return this->ref_count() > 1; }


    };


} // namespaces

#undef RCPTR_DEBUG
#undef RCOUT
#undef RCOUT2

#endif // s11n_net_sq3_refcount_REFCOUNT_HPP_INCLUDED
