#pragma once

#include <set>
#include <map>
#include <list>
#include <deque>
#include <tuple>
#include <vector>
#include <sstream>
#include <type_traits>

#include <boost/function.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>


#define ESCALATOR_ASSERT( predicate, message ) \
if ( !(predicate) ) \
{ \
    std::stringstream ss; \
    ss << "Escalator assertion failure: " << #predicate << ": " << message; \
    throw std::runtime_error( ss.str() ); \
}


namespace navetas { namespace escalator {
    template<typename T>
    class Optional
    {
    private:
        void assign(const T& val)
        {
            m_set = true;
            new (&m_val.data) T(val);
        }
        void assign(T&& val)
        {
            m_set = true;
            new (&m_val.data) T(std::move(val));
        }

    public:
        typedef T value_type;
        Optional() : m_set(false) {}
        Optional(const T& val)
        {
            assign(val);
        }
        Optional(T&& val) : m_set(true)
        {
            assign(std::move(val));
        }

        Optional(const Optional& other)
        {
            if(other.m_set) assign(other.get());
            else m_set = false;
        }
        Optional& operator=(const Optional& other)
        {
            reset();
            if(other.m_set) assign(other.get());
            else m_set = false;
        }
        Optional(Optional&& other)
        {
            if(other.m_set)
            {
                assign(std::move(other.get()));
                other.m_set = false;
            }
            else m_set = false;
        }
        Optional& operator=(Optional&& other)
        {
            reset();
            if(other.m_set)
            {
                assign(std::move(other.get()));
                other.m_set = false;
            }
            else m_set = false;
        }

        Optional& operator=(const T& val)
        {
            //Support assignment to this->get()
            if(getPtr() == &val) return *this;

            reset();
            assign(val);
            return *this;
        }
        Optional& operator=(T&& val)
        {
            //Support assignment to this->get()
            if(getPtr() == &val) return *this;

            reset();
            assign(std::move(val));
            return *this;
        }

        operator bool() { return m_set; }

        T& get()
        {
            if(!m_set) throw std::runtime_error( "Optional not set" );
            return *getPtr();
        }
        const T& get() const
        {
            if(!m_set) throw std::runtime_error( "Optional not set" );
            return *getPtr();
        }
        T* getPtr()
        {
            return reinterpret_cast<T*>(&m_val.data);
        }
        const T* getPtr() const
        {
            return reinterpret_cast<const T*>(&m_val.data);
        }

        T* operator->() { return &get(); }
        T& operator*() { return get(); }

        void reset()
        {
            if(m_set)
            {
                get().~T();
                m_set = false;
            }
        }

        ~Optional()
        {
            reset();
        }
    private:
// TODO: When we move to gcc 4.8, both will support alignas
#if defined(__clang__)
        struct alignas(std::alignment_of<T>::value) space_t
#else
        struct space_t
#endif
        {
            char data[sizeof(T)];
        }
#if !defined(__clang__)
        __attribute__ ((aligned))
#endif
        ;

        bool m_set;
        space_t m_val;
    };

    template<typename FunctorT, typename InputT>
    struct FunctorHelper
    {
        typedef decltype(std::declval<FunctorT>()( std::declval<InputT>() )) out_t;
    };

    template<typename Source, typename InputT>
    class CopyWrapper;

    template<typename Source, typename FunctorT, typename InputT, typename ElT>
    class MapWrapper;
    
    template<typename Source, typename FunctorT, typename InputT, typename InnerT, typename ElT>
    class FlatMapWrapper;
    
    template<typename Source, typename FunctorT, typename InputT, typename ElT, typename StateT>
    class MapWithStateWrapper;
    
    template<typename Source, typename FunctorT, typename ElT>
    class FilterWrapper;
    
    template<typename ElT, typename IterT>
    class IteratorWrapper;
    
    template<typename Container, typename ElT>
    class ContainerWrapper;

    enum SliceBehavior
    {
        RETURN_UPTO,
        ASSERT_WHEN_INSUFFICIENT
    };
    class SliceError : public std::range_error
    {
    public:
        SliceError( const std::string& what_arg ) : std::range_error( what_arg ) {}
        SliceError( const char* what_arg ) : std::range_error( what_arg ) {}
    };
    template<typename SourceT, typename ElT>
    class SliceWrapper;
    
    template<typename Source1T, typename El1T, typename Source2T, typename El2T>
    class ZipWrapper;

    template<typename R>
    struct remove_all_reference
    {
        typedef typename std::remove_const<typename std::remove_reference<R>::type>::type type;
    };

    template<typename R>
    struct remove_all_reference<std::reference_wrapper<R>>
    {
        typedef typename std::remove_const<R>::type type;
    };

    template<typename ContainerT>
    using WrappedContainerConstVRef = std::reference_wrapper<
                                          const typename remove_all_reference<
                                              typename ContainerT::value_type
                                          >::type>;
    template<typename ContainerT>
    using WrappedContainerVRef = std::reference_wrapper<
                                     typename remove_all_reference<
                                         typename ContainerT::value_type
                                     >::type>;


    template<typename ContainerT>
    ContainerWrapper<ContainerT, typename ContainerT::value_type>
    clift( ContainerT&& cont );

    template<typename ContainerT>
    IteratorWrapper<WrappedContainerConstVRef<ContainerT>,
                    typename ContainerT::const_iterator> 
    lift( const ContainerT& cont );
    
    template<typename ContainerT>
    IteratorWrapper<WrappedContainerVRef<ContainerT>,
                    typename ContainerT::iterator>
    mlift( ContainerT& cont );
    
    template<typename Iterator>
    IteratorWrapper<WrappedContainerConstVRef<Iterator>,
                    Iterator>
    lift( Iterator begin, Iterator end );

    class EmptyError : public std::range_error
    {
    public:
        EmptyError( const std::string& what_arg ) : std::range_error( what_arg ) {}
        EmptyError( const char* what_arg ) : std::range_error( what_arg ) {}
    };

    // Non-templatised marker trait for type trait functionality
    class Lifted {};
    
    template<typename BaseT, typename ElT>
    class ConversionsBase : public Lifted
    {
    protected:
        //BaseT always implements:
        //ElT next();
        //bool hasNext();
        BaseT& get() { return static_cast<BaseT&>(*this); }
        
    public:
        typedef ElT el_t;
        typedef typename remove_all_reference<ElT>::type value_type;

        std::vector<ElT> toVec()
        {
            std::vector<ElT> t;
            while ( get().hasNext() ) t.push_back( get().next() );
            return t;
        }
        
        template< class OutputIterator >
        void toContainer( OutputIterator v ) 
        {
            while ( get().hasNext() ) *v++ = get().next();
        }
        
        std::deque<ElT> toDeque()
        {
            std::deque<ElT> t;
            while ( get().hasNext() ) t.push_back( get().next() );
            return t;
        }
        
        std::list<ElT> toList()
        {
            std::list<ElT> t;
            while ( get().hasNext() ) t.push_back( get().next() );
            return t;
        }
        
        std::set<ElT> toSet()
        {
            std::set<ElT> t;
            while ( get().hasNext() ) t.insert( get().next() );
            return t;
        }
        
        std::multiset<ElT> toMultiSet()
        {
            std::multiset<ElT> t;
            while ( get().hasNext() ) t.insert( get().next() );
            return t;
        }
        
        template<typename FunctorT>
        bool forall( FunctorT fn )
        {
            bool pred = true;
            while ( pred && get().hasNext() ) pred &= fn( get().next() );
            return pred;
        }
        
        template<typename FunctorT>
        bool exists( FunctorT fn )
        {
            bool pred = false;
            while ( get().hasNext() ) pred |= fn( get().next() );
            return pred;
        }
        
        // TODO: Can this remain lifted?
        template<typename FunctorT>
        std::pair<std::vector<ElT>, std::vector<ElT>> partition( FunctorT fn )
        {
            std::pair<std::vector<ElT>, std::vector<ElT>> res;
            
            while ( get().hasNext() )
            {
                ElT val = std::move(get().next());
                if ( fn(val) ) res.first.push_back( std::move(val) );
                else res.second.push_back( std::move(val) );
            }
            
            return res;
        }
        
        // TODO: Can this remain lifted?
        template<typename FunctorT>
        std::pair<std::vector<ElT>, std::vector<ElT>> partitionWhile( FunctorT fn )
        {
            std::pair<std::vector<ElT>, std::vector<ElT>> res;
            
            bool inFirst = true;
            while ( get().hasNext() )
            {
                ElT val = get().next();
                if ( !fn(val) ) inFirst = false;
                
                if ( inFirst ) res.first.push_back( std::move(val) );
                else res.second.push_back( std::move(val) );
            }
            
            return res;
        }
        
        // TODO: This should probably be lazy
        template<typename FunctorT>
        std::vector<ElT> takeWhile( FunctorT fn ) { return partitionWhile(fn).first; }
        
        template<typename FunctorT>
        std::vector<ElT> dropWhile( FunctorT fn ) { return partitionWhile(fn).second; }
        
        template<typename FunctorT>
        MapWrapper<BaseT, FunctorT, ElT, typename FunctorHelper<FunctorT, ElT>::out_t> map( FunctorT fn )
        {
            return MapWrapper<BaseT, FunctorT, ElT, typename FunctorHelper<FunctorT, ElT>::out_t>( std::move(get()), fn );
        }

        CopyWrapper<BaseT, ElT> copyElements()
        {
            return CopyWrapper<BaseT, ElT>( std::move(get()) );
        }
        
        template<typename FromT, typename ToT>
        class CastFunctor
        {
        public:
            ToT operator()( const FromT v ) const { return static_cast<ToT>( v ); }
        };
        
        template<typename ToT>
        MapWrapper<BaseT, CastFunctor<ElT, ToT>, ElT, ToT> castElements()
        {
            CastFunctor<ElT, ToT> cf;
            return map( cf );
        }
        
        template<typename FunctorT>
        FilterWrapper<BaseT, FunctorT, ElT> filter( FunctorT fn )
        {
            return FilterWrapper<BaseT, FunctorT, ElT>( std::move(get()), fn );
        }
        
        typedef MapWithStateWrapper<BaseT, std::function<std::pair<ElT, size_t>(const ElT&, size_t&)>, ElT, std::pair<ElT, size_t>, size_t> zipWithIndexWrapper_t;
        zipWithIndexWrapper_t zipWithIndex()
        {
            return zipWithIndexWrapper_t(
                std::move(get()),
                []( const ElT& el, size_t& index ) { return std::make_pair( el, index++ ); },
                0 );
        }
        
        typedef MapWithStateWrapper<BaseT, std::function<std::tuple<ElT, ElT>( ElT, Optional<ElT>& state )>, ElT, std::tuple<ElT, ElT>, Optional<ElT>> sliding2_t;
        sliding2_t sliding2()
        {
            Optional<ElT> startState( get().next() );
            return sliding2_t(
                std::move(get()),
                []( ElT el, Optional<ElT>& state )
                {
                    std::tuple<ElT, ElT> tp = std::tuple<ElT, ElT>( state.get(), el );
                    state = el;
                    return tp;
                },
                startState );
        }
        
        template<typename OrderingF>
        ContainerWrapper<std::vector<ElT>, ElT> sortWith( OrderingF orderingFn )
        {
            std::vector<ElT> v = toVec();
            std::sort( v.begin(), v.end(), orderingFn );
            
            ContainerWrapper<std::vector<ElT>, ElT> vw( std::move(v) );
            
            return vw;
        }
        
        template<typename KeyF>
        ContainerWrapper<std::vector<ElT>, ElT> sortBy( KeyF keyFn )
        {
            std::vector<ElT> v = toVec();
            std::sort( v.begin(), v.end(), [keyFn]( const ElT& lhs, const ElT& rhs ) { return keyFn(lhs) < keyFn(rhs); } );
            
            ContainerWrapper<std::vector<ElT>, ElT> vw( std::move(v) );
            
            return vw;
        }

        ContainerWrapper<std::vector<ElT>, ElT> sort()
        {
            std::vector<ElT> v = toVec();
            std::sort( v.begin(), v.end(), [](const ElT& a, const ElT& b)
            {
                //May be asked to compare std::reference_wrappers around types
                //This doesn't seem to find the operator< by default,
                //probably as it's defined on the value_type as a class method
                //and C++ won't try the default operator& on the reference wrapper
                //and dig around.
                const value_type& v_a = a;
                const value_type& v_b = b;
                return v_a < v_b;
            });
            
            ContainerWrapper<std::vector<ElT>, ElT> vw( std::move(v) );
            
            return vw;
        }
        
        template<typename FunctorT>
        void foreach( FunctorT fn )
        {
            while ( get().hasNext() )
            {
                fn( get().next() );
            }
        }
        
        template<typename KeyFunctorT, typename ValueFunctorT>
        auto groupBy( KeyFunctorT keyFn, ValueFunctorT valueFn ) -> std::map<typename FunctorHelper<KeyFunctorT, ElT>::out_t, std::vector<typename FunctorHelper<ValueFunctorT, ElT>::out_t>>
        {
            typedef typename FunctorHelper<KeyFunctorT, ElT>::out_t key_t;
            typedef typename FunctorHelper<ValueFunctorT, ElT>::out_t value_type;
            
            std::map<key_t, std::vector<value_type>> grouped;
            while ( get().hasNext() )
            {
                auto v = get().next();
                auto key = keyFn(v);
                grouped[std::move(key)].push_back( valueFn(std::move(v)) );
            }
            
            return grouped;
        }
        
        // TODO : Note that this forces evaluation of the input stream
        // TODO: distinct should be wrappable into distinctWith using
        // std::less
        ContainerWrapper<std::vector<ElT>, ElT> distinct()
        {
            std::set<ElT> seen;
            std::vector<typename std::set<ElT>::iterator> ordering;
            while ( get().hasNext() )
            {
                auto res = seen.insert( std::move( get().next() ) );
                if ( res.second ) ordering.push_back( res.first );
            }
            
            std::vector<ElT> res;
            for ( auto& it : ordering )
            {
                res.push_back( std::move(*it) );
            }
            
            ContainerWrapper<std::vector<ElT>,  ElT> vw( std::move(res) );
            return vw;
        }
        
        template<typename SetOrdering>
        ContainerWrapper<std::vector<ElT>, ElT> distinctWith( SetOrdering cmp )
        {
            //Same pattern as distinct above
            std::set<ElT, SetOrdering> seen( cmp );
            std::vector<typename std::set<ElT>::iterator> ordering;
            while ( get().hasNext() )
            {
                auto res = seen.insert( std::move( get().next() ) );
                if ( res.second ) ordering.push_back( res.first );
            }
            
            std::vector<ElT> res;
            for ( auto& it : ordering )
            {
                res.push_back( std::move(*it) );
            }
            
            ContainerWrapper<std::vector<ElT>,  ElT> vw( std::move(res) );
            return vw;
        }
        
        template<typename FunctorT, typename AccT>
        AccT fold( AccT init, FunctorT fn )
        {
            while ( get().hasNext() )
            {
                init = fn( std::move(init), std::move(get().next()) );
            }
            return init;
        }
        
        template<typename Source2T>
        ZipWrapper<BaseT, ElT, typename std::remove_reference<Source2T>::type, typename std::remove_reference<Source2T>::type::el_t>
        zip( Source2T&& source2 )
        {
            typedef typename std::remove_reference<Source2T>::type Source2TNoRef;
            return ZipWrapper<BaseT
                             , ElT
                             , Source2TNoRef
                             , typename Source2TNoRef::el_t>( std::move(get()), std::forward<Source2T>(source2) );
        }
        
        SliceWrapper<BaseT, ElT> slice( size_t from, size_t to, SliceBehavior behavior=RETURN_UPTO )
        {
            return SliceWrapper<BaseT, ElT>( std::move(get()), from, to, behavior );
        }
        
        SliceWrapper<BaseT, ElT> drop( size_t num, SliceBehavior behavior=RETURN_UPTO )
        {
            return SliceWrapper<BaseT, ElT>( std::move(get()), num, std::numeric_limits<size_t>::max(), behavior );
        }
        
        SliceWrapper<BaseT, ElT> take( size_t num, SliceBehavior behavior=RETURN_UPTO )
        {
            return SliceWrapper<BaseT, ElT>( std::move(get()), 0, num, behavior );
        }
        
        size_t count()
        {
            size_t count = 0;
            while ( get().hasNext() )
            {
                get().next();
                count++;
            }
            return count;
        }
        
        value_type sum()
        {
            ESCALATOR_ASSERT( get().hasNext(), "Mean over insufficient items" );
            value_type acc = get().next();
            while ( get().hasNext() )
            {
                acc += get().next();
            }
            return acc;
        }
        
        value_type mean()
        {
            ESCALATOR_ASSERT( get().hasNext(), "Mean over insufficient items" );
            
            size_t count = 1;
            value_type acc = get().next();
            while ( get().hasNext() )
            {
                acc += get().next();
                count++;
            }
            
            return acc / static_cast<double>(count);
        }
        
        value_type median()
        {
            std::vector<ElT> values;
            size_t count = 0;
            while ( get().hasNext() )
            {
                values.push_back( get().next() );
                count++;
            }
            std::sort( values.begin(), values.end() );
            
            if ( count & 1 )
            {
                return values[count/2];
            }
            else
            {
                return (values[count/2] + values[(count/2)-1]) / 2.0;
            }
        }
        
        //TODO: Should these use boost::optional to work round init requirements?
        std::tuple<size_t, value_type> argMin()
        {
            bool init = true;
            value_type ext = value_type();
            
            size_t minIndex = 0;
            for ( int i = 0; get().hasNext(); ++i )
            {
                if ( init ) ext = std::move(get().next());
                else
                {
                    auto n = std::move(get().next());
                    if ( n < ext )
                    {
                        ext = std::move(n);
                        minIndex = i;
                    }
                }
                init = false;
            }
            return std::make_tuple( minIndex, std::move(ext) );
        }
        
        std::tuple<size_t, value_type> argMax()
        {
            bool init = true;
            value_type ext = value_type();
            
            size_t maxIndex = 0;
            for ( int i = 0; get().hasNext(); ++i )
            {
                if ( init ) ext = std::move(get().next());
                else
                {
                    auto n = std::move(get().next());
                    if ( n > ext )
                    {
                        ext = std::move(n);
                        maxIndex = i;
                    }
                }
                init = false;
            }
            return std::make_tuple( maxIndex, std::move(ext) );
        }
        
        value_type min() { return std::template get<1>(argMin()); }
        value_type max() { return std::template get<1>(argMax()); }
        
        std::string mkString( const std::string& sep )
        {
            std::stringstream ss;
            bool init = true;
            while ( get().hasNext() )
            {
                if ( !init ) ss << sep;
                auto val = std::move(get().next());
                ss << static_cast<const value_type&>(val);
                init = false;
            }
            return ss.str();
        }
        
        double increasing();
    };

    template<typename BaseT, typename ElT, typename ElIsLifted>
    class ConversionsImpl : public ConversionsBase<BaseT, ElT>
    {
    };
    
    template<typename BaseT, typename El1T, typename El2T>
    class ConversionsImpl<BaseT, std::tuple<El1T, El2T>, std::false_type> : public ConversionsBase<BaseT, std::tuple<El1T, El2T>>
    {
    public:
        std::map<El1T, El2T> toMap()
        {
            std::map<El1T, El2T> t;
            // NOTE: without the 'this->', Clang claims that 'get' is undeclared. Gcc makes a similar claim.
            while ( this->get().hasNext() )
            {
                auto n = std::move(this->get().next());
                t[std::move(std::get<0>(n))] = std::move(std::get<1>(n));
            }
            return t;
        }
        
        std::multimap<El1T, El2T> toMultiMap()
        {
            std::multimap<El1T, El2T> t;
            // NOTE: without the 'this->', Clang claims that 'get' is undeclared. Gcc makes a similar claim.
            while ( this->get().hasNext() )
            {
                auto n = std::move(this->get().next());
                t.insert( std::make_pair(std::move(std::get<0>(n)), std::move(std::get<1>(n)) ) );
            }
            return t;
        }
    };
    
    template<typename BaseT, typename ElT>
    class ConversionsImpl<BaseT, ElT, std::true_type> : public ConversionsBase<BaseT, ElT>
    {
    public:
        template<typename FunctorT>
        FlatMapWrapper<BaseT, FunctorT, ElT, typename ElT::el_t, typename FunctorHelper<FunctorT, typename ElT::el_t>::out_t> flatMap( FunctorT fn )
        {
            return FlatMapWrapper<BaseT, FunctorT, ElT, typename ElT::el_t, typename FunctorHelper<FunctorT, typename ElT::el_t>::out_t>( std::move(this->get()), fn );
        }

        template<typename T>
        class Identity
        {
        public:
            T operator()( T t ) const { return t; }
        };
        
        FlatMapWrapper<BaseT, Identity<typename ElT::el_t>, ElT, typename ElT::el_t, typename ElT::el_t> flatten()
        {
            return FlatMapWrapper<BaseT, Identity<typename ElT::el_t>, ElT, typename ElT::el_t, typename ElT::el_t>( std::move(this->get()), Identity<typename ElT::el_t>() );
        }
    };
    
    template<typename BaseT, typename ElT>
    class Conversions : public ConversionsImpl<BaseT, ElT, typename std::is_base_of<Lifted, ElT>::type>
    {
    };
    
    template<typename Source, typename FunctorT, typename ElT>
    class FilterWrapper : public Conversions<FilterWrapper<Source, FunctorT, ElT>, ElT>
    {
    public:
        FilterWrapper( const Source& source, FunctorT fn ) : m_source(source), m_fn(fn)
        {
            populateNext();
        }

        FilterWrapper( Source&& source, FunctorT fn ) : m_source(std::move(source)), m_fn(fn)
        {
            populateNext();
        }

        ElT next()
        {
            ElT v = std::move(m_next.get());
            populateNext();
            return v;
        }
        
        bool hasNext() { return m_next; }
        
    private:
        void populateNext()
        {
            m_next.reset();
            while ( m_source.hasNext() )
            {
                ElT next = std::move(m_source.next());
                if ( m_fn( next ) )
                {
                    m_next = std::move(next);
                    break;
                }
            }
        }
    
    private:
        Source                  m_source;
        FunctorT                m_fn;
        Optional<ElT>           m_next;
    };

    template<typename Source, typename FunctorT, typename InnerT, typename InputT, typename ElT>
    class FlatMapWrapper : public Conversions<FlatMapWrapper<Source, FunctorT, InnerT, InputT, ElT>, ElT>
    {
    public:
        FlatMapWrapper( const Source& source, FunctorT fn ) : m_source(source), m_fn(fn)
        {
            populateNext();
        }

        ElT next()
        {
            ElT res = m_fn( std::move(m_next.get()) ); //Moves through here OK
            populateNext();
            return res;
        }
        
        bool hasNext() { return m_next; }
        
    private:
        void populateNext()
        {
            m_next.reset();
            while ( (!m_inner || !m_inner->hasNext()) && m_source.hasNext() )
            {
                m_inner = std::move(m_source.next());
            }
            
            if ( m_inner && m_inner->hasNext() )
            {
                m_next = std::move(m_inner->next());
            }
        }
    
    private:
        typedef std::function<ElT(InputT)> FunctorHolder_t;
        Source                      m_source;
        FunctorHolder_t             m_fn;
        Optional<InnerT>            m_inner;
        Optional<InputT>            m_next;
    };

    template<typename Source, typename InputT>
    class CopyWrapper : public Conversions<CopyWrapper<Source, InputT>,
                                           typename std::remove_const<typename InputT::type>::type>
    {
    private:
        typedef CopyWrapper<Source, InputT> self_t;
    public:
        CopyWrapper( const Source& source ) : m_source(source) {}
        CopyWrapper( Source&& source ) : m_source(std::move(source)) {}
        bool hasNext() { return m_source.hasNext(); }
        typename std::remove_const<typename InputT::type>::type next() { return m_source.next(); }
    private:
        Source m_source;
    };

    template<typename Source, typename FunctorT, typename InputT, typename ElT>
    class MapWrapper : public Conversions<MapWrapper<Source, FunctorT, InputT, ElT>, ElT>
    {
    private:
        typedef MapWrapper<Source, FunctorT, InputT, ElT> self_t;
    public:
        MapWrapper( const Source& source, FunctorT fn ) : m_source(source), m_fn(fn)
        {
        }

        MapWrapper( Source&& source, FunctorT fn ) : m_source(std::move(source)), m_fn(fn)
        {
        }

        bool hasNext() { return m_source.hasNext(); }
        ElT next() { return m_fn( std::move(m_source.next()) ); }
    
    private:
        typedef std::function<ElT(InputT)> FunctorHolder_t;
    
        Source              m_source;
        FunctorHolder_t     m_fn;
    };
    
    template<typename Source1T, typename El1T, typename Source2T, typename El2T>
    class ZipWrapper : public Conversions<ZipWrapper<Source1T, El1T, Source2T, El2T>, std::tuple<El1T, El2T>>
    {
    public:
        ZipWrapper( const Source1T& source1, const Source2T& source2 ) : m_source1(source1)
                                                                       , m_source2(source2)
        {
        }
        
        ZipWrapper( Source1T&& source1, Source2T&& source2 ) : m_source1(std::move(source1))
                                                             , m_source2(std::move(source2))
        {
        }

        bool hasNext() { return m_source1.hasNext() && m_source2.hasNext(); }
        std::tuple<El1T, El2T> next() { return std::make_tuple( std::move(m_source1.next()), std::move(m_source2.next()) ); }
        
    private:
        Source1T m_source1;
        Source2T m_source2;
    };
    
    template<typename Source, typename FunctorT, typename InputT, typename ElT, typename StateT>
    class MapWithStateWrapper : public Conversions<MapWithStateWrapper<Source, FunctorT, InputT, ElT, StateT>, ElT>
    {
    public:
        typedef MapWithStateWrapper<Source, FunctorT, InputT, ElT, StateT> self_t;
        
        MapWithStateWrapper( const Source& source, FunctorT fn, StateT state ) : m_source(source), m_fn(fn), m_state(state)
        {
        }

        MapWithStateWrapper( Source&& source, FunctorT fn, StateT state ) : m_source(std::move(source)), m_fn(fn), m_state(state)
        {
        }
        
        bool hasNext() { return m_source.hasNext(); }
        ElT next() { return m_fn( std::move(m_source.next()), m_state ); }
    
    private:
        Source      m_source;
        FunctorT    m_fn;
        StateT      m_state;
    };

    /**
     * std::reference_wrapper<T> won't auto-convert to a
     * std::reference_wrapper<const T>. Use this function to get
     * the reference inside whether or not there is a reference wrapper.
     */
    template<typename R>
    static R& StripReferenceWrapper(R& v) { return v; }
    template<typename R>
    static R& StripReferenceWrapper(std::reference_wrapper<R>& v) { return v.get(); }
    template<typename R>
    static const R& StripReferenceWrapper(const R& v) { return v; }
    template<typename R>
    static const R& StripReferenceWrapper(const std::reference_wrapper<R>& v) { return v.get(); }

    template<typename ElT, typename IterT>
    class IteratorWrapper : public Conversions<IteratorWrapper<ElT, IterT>, ElT>
    {
    public:
        typedef IteratorWrapper<ElT, IterT> self_t;
        typedef ElT el_t;
        
        IteratorWrapper( IterT start, IterT end ) : m_iter(start), m_end(end)
        {
        }
        
        bool hasNext()
        {
            return m_iter != m_end;
        }
        
        ElT next()
        {
            IterT curr = m_iter++;
            return StripReferenceWrapper(*curr);
        }

    private:
        IterT m_iter;
        IterT m_end;
    };
    
    template<typename SourceT, typename ElT>
    class SliceWrapper : public Conversions<SliceWrapper<SourceT, ElT>, ElT>
    {
    public:
        SliceWrapper( const SourceT& source, size_t from, size_t to, SliceBehavior behavior )
            : m_source(source), m_from(from), m_to(to), m_count(0), m_behavior( behavior )
        {
            initiate(from, to, behavior);
        }

        SliceWrapper( SourceT&& source, size_t from, size_t to, SliceBehavior behavior )
            : m_source(std::move(source)), m_from(from), m_to(to), m_count(0), m_behavior( behavior )
        {
            initiate(from, to, behavior);
        }

        bool hasNext()
        {
            if(m_count==m_to) return false;

            if(m_behavior == ASSERT_WHEN_INSUFFICIENT)
            {
                if(!m_source.hasNext())
                {
                    throw SliceError( "Iterator unexpectedly exhausted" );
                }
                else
                {
                    return true;
                }
            }
            else
            {
                return m_source.hasNext();
            }
        }
        
        ElT next()
        {
            m_count++;
            ESCALATOR_ASSERT( m_source.hasNext(), "Iterator exhausted" );
            return m_source.next();
        }
        
    private:
        void initiate( size_t from, size_t to, SliceBehavior behavior )
        {
            while ( m_source.hasNext() && m_count < m_from )
            {
                m_source.next();
                m_count++;
            }

            if(m_behavior == ASSERT_WHEN_INSUFFICIENT && m_count < m_from)
            {
                throw SliceError( "Iterator unexpectedly exhausted" );
            }
        }
        
        SourceT     m_source;
        size_t      m_from;
        size_t      m_to;
        size_t      m_count;
        SliceBehavior m_behavior;
    };
    
    template<typename Container, typename ElT>
    class ContainerWrapper : public Conversions<ContainerWrapper<Container, ElT>, ElT>
    {
    public:
        typedef typename Container::iterator iterator;
        
        ContainerWrapper( Container&& data ) : m_data(std::move(data)), m_iter(m_data.begin()), m_end(m_data.end()), m_counter(0)
        {
        }
        
        ContainerWrapper( const Container& data ) : m_data(data), m_iter(m_data.begin()), m_end(m_data.end()), m_counter(0)
        {
        }

        //When copying, copy data that is left to be consumed only
        //Then new object delivers all data in its container
        ContainerWrapper( const ContainerWrapper& other )
        {
            for( iterator it = other.m_iter; it != other.m_end; it++ )
            {
                m_data.push_back( *it );
            }

            m_iter = m_data.begin();
            m_end = m_data.end();
            m_counter = 0;
        }

        ContainerWrapper( ContainerWrapper&& other ) : m_data(std::move(other.m_data))
                                                     , m_iter(m_data.begin())
                                                     , m_end(m_data.end())
                                                     , m_counter(other.m_counter)
        {
            for(size_t i=0; i<m_counter; i++)
            {
                m_iter++;
            }
        }

        ContainerWrapper& operator=( const ContainerWrapper& other )
        {
            m_data.clear();

            for( iterator it = other.m_iter; it != other.m_end; it++ )
            {
                m_data.push_back( *it );
            }

            m_iter = m_data.begin();
            m_end = m_data.end();
            m_counter = 0;

            return *this;
        }

        ContainerWrapper& operator=( ContainerWrapper&& other )
        {
            //Can't move into the already existing m_data
            //Move elements individually
            m_data.clear();

            for( iterator it = other.m_iter; it != other.m_end; it++ )
            {
                m_data.push_back( std::move(*it) );
            }

            m_iter = m_data.begin();
            m_end = m_data.end();
            m_counter = 0;

            return *this;
        }
        
        bool hasNext()
        {
            return m_iter != m_end;
        }
        
        ElT& next()
        {
            ESCALATOR_ASSERT( m_iter != m_end, "Iterator exhausted" );
            iterator curr = m_iter++;
            m_counter++;
            return *curr;
        }
        
        operator const Container&() { return m_data; }
        
    protected:
        Container       m_data;
        iterator        m_iter;
        iterator        m_end;
        //Track how many have been consumed to aid
        //the move constructor
        size_t          m_counter;
    };

    class Counter : public Conversions<Counter, int>
    {
    public:
        Counter() : m_count(0) {}
        
        bool hasNext() { return true; }
        
        int next()
        {
            return m_count++;
        }
        
    private:
        int m_count;
    };

    template< typename StreamT >
    class StreamWrapper : public Conversions<StreamWrapper<StreamT>, typename StreamT::value_type>
    {
    public:
        StreamWrapper( StreamT& stream ) : m_stream(stream) {}
        
        bool hasNext() { return !m_stream.eof(); }
        
        typename StreamT::value_type next()
        {
            return m_stream.pop();
        }
        
    private:
        StreamT&       m_stream;
    };

    template< typename OptionalT >
    class OptionalWrapper : public Conversions<OptionalWrapper<OptionalT>, typename OptionalT::value_type>
    {
    public:
        OptionalWrapper( OptionalT&& op ) : m_op( std::move(op) ) {}
        bool hasNext() { return m_op; }
        typename OptionalT::value_type next()
        {
            typename OptionalT::value_type val = std::move(m_op.get());
            m_op.reset();
            return val;
        }
    private:
        OptionalT m_op;
    };

    class IStreamWrapper : public Conversions<IStreamWrapper, std::string>
    {
    public:
        IStreamWrapper( std::istream& stream ) : m_stream(stream)
        {
            populateNext();
        }
        
        bool hasNext() { return m_hasNext; }
        
        std::string next()
        {
            std::string curr = std::move(m_currLine);
            populateNext();
            return curr;
        }
        
    private:
        void populateNext()
        {
            m_hasNext = std::getline( m_stream, m_currLine );
        }
        
    private:
        std::istream&       m_stream;
        bool                m_hasNext;
        std::string         m_currLine;
    };
    
    class StringWrapper : public ContainerWrapper<std::string, char>
    {
    public:
        StringWrapper( const std::string& data ) : ContainerWrapper(data)
        {
        }
        
        StringWrapper trim()
        {
            return StringWrapper( boost::algorithm::trim_copy(m_data) );
        }
        
        ContainerWrapper<std::vector<std::string>, std::string> split( const std::string& splitChars )
        {
            std::vector<std::string> splitVec;
            
            boost::algorithm::split( splitVec, m_data, boost::algorithm::is_any_of(splitChars) );
            
            return ContainerWrapper<std::vector<std::string>, std::string>( std::move(splitVec) );
        }
        
        const std::string& toString() { return m_data; }
    };
    
    template<typename BaseT, typename ElT>
    double ConversionsBase<BaseT, ElT>::increasing()
    {
        std::vector<double> res = zipWithIndex()
            .sortWith( []( const std::tuple<ElT, size_t>& a, const std::tuple<ElT, size_t>& b ) { return std::get<0>(a) < std::get<0>(b); } )
            .map( []( const std::tuple<ElT, size_t>& v ) { return std::get<1>(v); } )
            .zipWithIndex()
            .map( []( const std::tuple<size_t, size_t>& v ) { return std::abs( static_cast<double>( std::get<0>(v) ) - static_cast<double>( std::get<1>(v) ) ); } )
            .toVec();
            
        double scale = static_cast<double>(res.size() * res.size()) / 2.0;
        
        return 1.0 - (lift(res).sum() / scale);
    }
    
    inline Counter counter() { return Counter(); }
    inline IStreamWrapper lift( std::istream& data ) { return IStreamWrapper(data); }
    inline StringWrapper lift( const std::string& data ) { return StringWrapper(data); }
    
    template<typename StreamT>
    StreamWrapper<StreamT> slift( StreamT& stream ) { return StreamWrapper<StreamT>( stream ); }

    template<typename ContainerT>
    ContainerWrapper<ContainerT, typename ContainerT::value_type>
    clift( ContainerT&& cont )
    {
        return ContainerWrapper<ContainerT, typename ContainerT::value_type>( std::forward<ContainerT>(cont) );
    }

    template<typename ElT>
    inline OptionalWrapper<Optional<ElT>>
    clift( Optional<ElT>&& op )
    {
        return OptionalWrapper<Optional<ElT>>( std::forward<Optional<ElT>>(op) );
    }

    template<typename ContainerT>
    IteratorWrapper<WrappedContainerConstVRef<ContainerT>,
                    typename ContainerT::const_iterator> 
    lift( const ContainerT& cont )
    {
        return IteratorWrapper<WrappedContainerConstVRef<ContainerT>,
                               typename ContainerT::const_iterator>( cont.begin(), cont.end() );
    } 

    template<typename ContainerT>
    IteratorWrapper<WrappedContainerVRef<ContainerT>,
                    typename ContainerT::iterator>
    mlift( ContainerT& cont )
    {
        return IteratorWrapper<WrappedContainerVRef<ContainerT>,
                               typename ContainerT::iterator>( cont.begin(), cont.end() );
    }
    
    template<typename Iterator>
    IteratorWrapper<WrappedContainerConstVRef<Iterator>,
                    Iterator>
    lift( Iterator begin, Iterator end )
    {
        return IteratorWrapper<WrappedContainerConstVRef<Iterator>,
                               Iterator>( begin, end );
    }
}}
