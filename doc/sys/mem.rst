Memory management functions
===========================

The document describes the facilities implemented for managing virtual and physical
memory and related resources.

All virtual memory is split into two spaces: kernel and user. ``0xFFFFFF0000000000``
is the boundary, any addresses beyond which are considered kernel-space and any address
lower which is user-space.

Physical memory management
--------------------------

Kernel functions can allocate physical memory using the following two functions::

    uintptr_t mm_phys_alloc_page(void);
    uintptr_t mm_phys_alloc_contiguous(size_t count);

These two functions return physical pointers (or ``MM_NADDR`` in case of failure).
The first one allocates and returns a single page, whereas the latter one attempts
an allocation of contiguous physical range of ``count`` 4KiB pages.

After usage, the pages can be freed using ``mm_phys_free_page()``::

    void mm_phys_free_page(uintptr_t addr)

This function only deallocates a single page, so for contiguous ranges, a loop is
required::

    for (size_t i = 0; i < count; ++i) {
        mm_phys_free_page(addr + i * MM_PAGE_SIZE);
    }

Kernel heap
-----------

For small and frequent allocations, heap is available::

    void *kmalloc(size_t size);
    void kfree(void *ptr);

These two functions should work exactly as ``malloc(3)``/``free(3)`` everyone's
familiar with, so no further description is needed.

Kernel virtual memory management
--------------------------------

Function useful in developing kernel features are described here. For userspace virtual
memory facilities see `Userspace memory management`_.

The kernel has lower 1GiB of physical memory mapped at ``0xFFFFFF0000000000``, which
allows for easier access to physical memory without needing to map it first.

``MM_VIRTUALIZE(addr)`` macro is used to convert a physical memory address into a
kernel-space pointer.

Likewise, ``MM_PHYS(addr)`` macro converts the virtual address to physical one.

.. warning ::
    These functions don't provide any boundary check and may cause undefined behavior
    if provided invalid input.

Userspace memory management
---------------------------

Userspace memory consists of two kinds of virtual regions:

1. Unique ``virt`` -> ``phys`` mappings. These always correspond to unqiue physical
   pages.
2. Shared memory regions (``mmap()`` ed or any other kind). These may refer either
   to shared physical memory regions, or can be file/device-mapped.

Primary function for manipulating non-shared mappings is ``mm_map_single()`` function::

    int mm_map_single(mm_space_t space, uintptr_t virt, uintptr_t phys, uint64_t flags);

The function is used to map a single virtual memory page to a physical address, creating
``virt .. virt + MM_PAGE_SIZE`` -> ``phys .. phys + MM_PAGE_SIZE`` association. Currently,
only 4KiB pages can be mapped this way. The ``flags`` parameter controls permission bits
for the page and can be one of the following:

* ``MM_PAGE_USER``  --- the page is accessible from userspace code
* ``MM_PAGE_WRITE`` --- the page is writable
* ``MM_PAGE_EXEC``  --- code execution is allowed for this page

A mapped address can be queried for its corresponding physical address using
``mm_map_get()``::

    uintptr_t mm_map_get(mm_space_t space, uintptr_t virt, uint64_t *flags)

Corresponding physical address is returned on success, optionally setting ``*flags``
to mapping permission bits. In case of failure (the address is not mapped), ``MM_NADDR``
is returned.

Finally, once the virtual mapping is no longer needed, it can be removed from
process' virtual address tables using ``mm_umap_single``::

    uintptr_t mm_umap_single(mm_space_t space, uintptr_t virt, uint32_t size);

``size`` parameter here limits which kinds of pages are unmapped, returning ``MM_NADDR``
if trying, for example, to unmap a 4KiB page, but the virtual address actually represens
2MiB page. ``size`` takes the following values:

* 0 --- Any mapping is removed
* 1 --- 4KiB mapping is removed

On success, this function will return physical memory page address which was referred
to by ``virt`` in the memory space. Otherwise, ``MM_NADDR`` is reported.

Contiguous regions in memory spaces can be bound to physical memory using ``vmfind()``
and ``vmalloc()`` functions::

    uintptr_t vmfind(const mm_space_t pd, uintptr_t from, uintptr_t to, size_t npages);
    uintptr_t vmalloc(mm_space_t pd, uintptr_t from, uintptr_t to, size_t npages, uint64_t flags);

``vmfind()`` searches for a contiguous free space of ``npages`` * 4KiB pages in
``from .. to`` range and returns it. ``vmalloc()`` does the same, additionally mapping
the region to a set of physical pages with given permission ``flags``. Both functions
return ``MM_NADDR`` in case of failure.

Then, ``vmfree()`` function can be used to release the region, unmapping and freeing
physical pages.
