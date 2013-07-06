	//! Stop the default std::vector<> doing unaligned storage
	template<> class vector<TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE, allocator<TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE>> : public vector<TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE, NiallsCPP11Utilities::aligned_allocator<TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE>>
	{
		typedef vector<TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE, NiallsCPP11Utilities::aligned_allocator<TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE>> Base;
	public:
		explicit vector (const allocator_type& alloc = allocator_type()) : Base(alloc) { }
		explicit vector (size_type n) : Base(n) { }
		vector (size_type n, const value_type& val,
			const allocator_type& alloc = allocator_type()) : Base(n, val, alloc) { }
		template <class InputIterator> vector (InputIterator first, InputIterator last,
			const allocator_type& alloc = allocator_type()) : Base(first, last, alloc) { }
		vector (const vector& x) : Base(x) { }
		//vector (const vector& x, const allocator_type& alloc) : Base(x, alloc) { }
		vector (vector&& x) : Base(std::move(x)) { }
		//vector (vector&& x, const allocator_type& alloc) : Base(std::move(x), alloc) { }
#if defined(_MSC_VER) && _MSC_VER>1700
		vector (initializer_list<value_type> il, const allocator_type& alloc = allocator_type()) : Base(il, alloc) { }
#endif
	};
