/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee++ Internals &mdash; Memory Pool Implementation
 * @internal
 *
 * @sa memory_pool.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/internal/catch.hpp>
#include <ironbeepp/internal/data.hpp>
#include <ironbeepp/internal/throw.hpp>

#include <ironbee/mpool.h>
#include <ironbee/debug.h>

#include <cassert>

namespace IronBee {

namespace Hooks {
namespace {

extern "C" {

ib_status_t cleanup(
    void* cbdata
)
{
    IB_FTRACE_INIT();

    // We have no engine...
    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(NULL,
        Internal::data_to_value<MemoryPool::cleanup_t>(cbdata)()
    ));

    // Now we need to clear our own callback data.
    delete reinterpret_cast<boost::any*>(cbdata);

    IB_FTRACE_RET_STATUS(IB_OK);
}

} // extern "C"
}
} // Hooks

namespace Internal {
namespace {

MemoryPool create_memory_pool(
    const char* name,
    ib_mpool_t* parent = NULL,
    size_t      size   = 0
)
{
    ib_mpool_t* ib_mpool = NULL;
    ib_status_t rc = ib_mpool_create_ex(
        &ib_mpool,
        name,
        parent,
        (size != 0 ? size : 1024)
    );

    Internal::throw_if_error(rc);
    assert(ib_mpool != NULL);

    return MemoryPool(ib_mpool);
}

}
} // Internal

/* ConstMemoryPool */

ConstMemoryPool::ConstMemoryPool() :
    m_ib(NULL)
{
    // nop
}

ConstMemoryPool::ConstMemoryPool(const ib_mpool_t* ib_mpool) :
    m_ib(ib_mpool)
{
    // nop
}

const char* ConstMemoryPool::name() const
{
    return ib_mpool_name(ib());
}

/* MemoryPool */

// See api documentation for discussion of const_cast.

MemoryPool MemoryPool::remove_const(const ConstMemoryPool& cmp)
{
    return MemoryPool(const_cast<ib_mpool_t*>(cmp.ib()));
}

MemoryPool::MemoryPool() :
    m_ib(NULL)
{
    // nop
}

MemoryPool MemoryPool::create()
{
    return create("MemoryPool");
}

MemoryPool MemoryPool::create(
    const char* name,
    size_t      size
)
{
    return Internal::create_memory_pool(name, NULL, size);
}

MemoryPool MemoryPool::create(
    const char* name,
    MemoryPool  parent,
    size_t      size
)
{
    if (! parent) {
        BOOST_THROW_EXCEPTION(
          einval() << errinfo_what(
            "Singular parent provided to memory pool."
          )
        );

    }
    return Internal::create_memory_pool(name, parent.ib(), size);
}

MemoryPool MemoryPool::create_subpool() const
{
    return create("SubPool", *this);
}

MemoryPool MemoryPool::create_subpool(
    const char* subpool_name,
    size_t      size
) const
{
    return create(subpool_name, *this, size);
}


void* MemoryPool::alloc(size_t size) const
{
    void* memory = ib_mpool_alloc(ib(), size);
    if (! memory) {
        BOOST_THROW_EXCEPTION(
          ealloc() << errinfo_what(
            "ib_mpool_alloc() returned NULL"
          )
        );
    }
    return memory;
}

void* MemoryPool::calloc(size_t count, size_t size) const
{
    void* memory = ib_mpool_calloc(ib(), count, size);
    if (! memory) {
        BOOST_THROW_EXCEPTION(
          ealloc() << errinfo_what(
            "ib_mpool_calloc() returned NULL"
          )
        );
    }
    return memory;
}

void* MemoryPool::calloc(size_t size) const
{
    return calloc(1, size);
}

void MemoryPool::clear() const
{
    ib_mpool_clear(ib());
}

void MemoryPool::destroy() const
{
    ib_mpool_destroy(ib());
}

void MemoryPool::register_cleanup(cleanup_t f) const
{
    // We can't use this as the memory pool for value_to_data because then
    // the callback would be deleted before it is called.  The callback
    // itself will free it's own data.
    ib_status_t rc = ib_mpool_cleanup_register(
        ib(),
        Hooks::cleanup,
        Internal::value_to_data(
            f,
            NULL // See above
        )
    );
    Internal::throw_if_error(rc);
}

MemoryPool::MemoryPool(ib_mpool_t* ib_mpool) :
    ConstMemoryPool(ib_mpool),
    m_ib(ib_mpool)
{
    // nop
}

/* Global */

std::ostream& operator<<(std::ostream& o, const ConstMemoryPool& memory_pool)
{
    if (! memory_pool) {
        o << "IronBee::MemoryPool[!singular!]";
    }
    else {
        o << "IronBee::MemoryPool[" << memory_pool.name() << "]";
    }
    return o;
}

/* ScopedMemoryPool */

ScopedMemoryPool::ScopedMemoryPool() :
    m_pool(MemoryPool::create("ScopedMemoryPool"))
{
    // nop
}

ScopedMemoryPool::ScopedMemoryPool(const char* name, size_t size) :
    m_pool(MemoryPool::create(name, size))
{
    // nop
}

ScopedMemoryPool::~ScopedMemoryPool()
{
    m_pool.destroy();
}

};
