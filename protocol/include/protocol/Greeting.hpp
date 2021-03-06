
/**
 *  @file Greeting.hpp
 *  
 *  Mysql 握手协议包 S -> C
 * 
 *  Copyright © 2020 by LiuJ
 */

#pragma once

#include <unistd.h>
#include <vector>
#include <iostream>
#include <memory>

namespace Protocol {

    class GreetingPacket {
    
    public:
        ~GreetingPacket() noexcept;
        GreetingPacket(const GreetingPacket&) = delete;
        GreetingPacket(GreetingPacket&&) noexcept = delete;
        GreetingPacket& operator=(const GreetingPacket&) = delete;
        GreetingPacket& operator=(GreetingPacket&&) noexcept = delete;
        GreetingPacket();

        GreetingPacket(uint32_t connectionID, std::string serverVersion);
        /**
         * 编码 greeting 包
         */
        std::vector< uint8_t > Pack();
        
        std::string GetServerVersion();
        uint32_t GetConnectionID();
        std::string GetAuthPluginName();
        uint16_t GetServerStatus();

        /**
         * 解码 greeting 包
         */
        bool UnPack(std::vector< uint8_t >& packet);

    private:

        struct Impl;

        std::unique_ptr< Impl > impl_;

    };

}