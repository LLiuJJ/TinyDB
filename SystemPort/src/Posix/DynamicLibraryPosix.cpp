/**
 * @file DynamicLibraryPosix.cpp
 *
 * This module contains the POSIX implementation of the
 * SystemAbstractions::DynamicLibrary class.
 *
 * Copyright (c) 2020 by LiuJ
 * 
 */

#include "DynamicLibraryImpl.hpp"

#include <assert.h>
#include <dlfcn.h>
#include <string>
#include <CppStringPlus/CppStringPlus.hpp>
#include <sys/param.h>
#include <SystemPort/DynamicLibrary.hpp>
#include <unistd.h>
#include <vector>

namespace SystemAbstractions {

    DynamicLibrary::DynamicLibrary()
        : impl_(new DynamicLibraryImpl())
    {
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept 
        : impl_(std::move(other.impl_))
    {
    }

    DynamicLibrary::~DynamicLibrary() {
        if (impl_ == nullptr) {
            return;
        }
        Unload();
    }

    // 重载 赋值运算符
    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
        assert(this != &other);
        impl_ = std::move(other.impl_);
        return *this;
    }

    void DynamicLibrary::Unload() {
        if (impl_->libraryHandle != NULL) {
            (void)dlclose(impl_->libraryHandle);
            impl_->libraryHandle = NULL;
        }
    }

    bool DynamicLibrary::Load(const std::string& path, const std::string& name) {
        Unload();
        std::vector< char > originalPath(MAXPATHLEN);
        (void)getcwd(&originalPath[0], originalPath.size());
        (void)chdir(path.c_str());
        const auto library = CppStringPlus::sprintf(
            "%s/lib%s.%s",
            path.c_str(),
            name.c_str(),
            DynamicLibraryImpl::GetDynamicLibraryFileExtension().c_str()
        );
        impl_->libraryHandle = dlopen(library.c_str(), RTLD_NOW);
        (void)chdir(&originalPath[0]);
        return (impl_->libraryHandle != NULL);
    }

    void* DynamicLibrary::GetProcedure(const std::string& name) {
        return dlsym(impl_->libraryHandle, name.c_str());
    }

    std::string DynamicLibrary::GetLastError() {
        return dlerror();
    }
    
}