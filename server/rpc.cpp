//
// Copyright (c) 2018-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/BeastLounge
//

#include "rpc.hpp"
#include <boost/beast/core/error.hpp>
#include <type_traits>

//------------------------------------------------------------------------------

namespace {

class rpc_error_codes : public beast::error_category
{
public:
    const char*
    name() const noexcept override
    {
        return "beast-lounge";
    }

    std::string
    message(int ev) const override
    {
        switch(static_cast<rpc_error>(ev))
        {
        case rpc_error::parse_error: return
            "An error occurred on the server while parsing the JSON text.";
        case rpc_error::invalid_request: return
            "The JSON sent is not a valid Request object";
        case rpc_error::method_not_found: return
            "The method does not exist or is not available";
        case rpc_error::invalid_params: return
            "Invalid method parameters";
        case rpc_error::internal_error: return
            "Internal JSON-RPC error";

        case rpc_error::expected_object: return
            "Expected object in JSON-RPC request";
        case rpc_error::expected_string_version: return
            "Expected string version in JSON-RPC request";
        case rpc_error::unknown_version: return
            "Uknown version in JSON-RPC request";
        case rpc_error::invalid_null_id: return
            "Invalid null id in JSON-RPC request";
        case rpc_error::expected_strnum_id: return
            "Expected string or number id in JSON-RPC request";
        case rpc_error::expected_id: return
            "Missing id in JSON-RPC request version 1";
        case rpc_error::missing_method: return
            "Missing method in JSON-RPC request";
        case rpc_error::expected_string_method: return
            "Expected string method in JSON-RPC request";
        case rpc_error::expected_structured_params: return
            "Expected structured params in JSON-RPC request version 2";
        case rpc_error::missing_params: return
            "Missing params in JSON-RPC request version 1";
        case rpc_error::expected_array_params: return
            "Expected array params in JSON-RPC request version 1";
        }
        if( ev >= -32099 &&
            ev <= -32099)
            return "An implementation defined server error was received";
        return "Unknown RPC error #" + std::to_string(ev);
    }

    beast::error_condition
    default_error_condition(int ev) const noexcept override
    {
        return {ev, *this};
    }
};

} // (anon)

beast::error_code
make_error_code(rpc_error e)
{
    static rpc_error_codes const cat{};
    return {static_cast<std::underlying_type<
        rpc_error>::type>(e), cat};
}

//------------------------------------------------------------------------------

json::value
rpc_exception::
to_json(
    boost::optional<json::value> const& id) const
{
    json::value jv;
    jv["jsonrpc"] = "2.0";
    auto& err = jv["error"].emplace_object();
    err["code"] = code_;
    err["message"] = msg_;
    if(id.has_value())
        jv["id"] = *id;
    return jv;
}

//------------------------------------------------------------------------------

rpc_request::
rpc_request(json::storage_ptr sp)
    : method(json::allocator<char>(sp))
    , params(sp)
    , id(std::move(sp))
{
}

void
rpc_request::
extract(
    json::value&& jv,
    beast::error_code& ec)
{
    // clear the fields first
    method = json::string(
        json::allocator<char>(
            jv.get_storage()));
    params = json::value(
        json::kind::null,
        jv.get_storage());
    id = json::value(
        json::kind::null,
        jv.get_storage());

    // must be object
    if(! jv.is_object())
    {
        ec = rpc_error::expected_object;
        return ;
    }

    auto& obj = jv.as_object();

    // extract id first so on error,
    // the response can include it.
    {
        auto it = obj.find("id");
        if(it != obj.end())
            id.emplace(std::move(it->second));
    }

    // now check the version
    {
        auto it = obj.find("jsonrpc");
        if(it != obj.end())
        {
            if(! it->second.is_string())
            {
                ec = rpc_error::expected_string_version;
                return;
            }
            auto const& s =
                it->second.as_string();
            if(s != "2.0")
            {
                ec = rpc_error::unknown_version;
                return;
            }
            version = 2;
        }
        else
        {
            version = 1;
        }
    }

    // validate the extracted id
    {
        if(version == 2)
        {
            if(id.has_value())
            {
                // The use of Null as a value for the
                // id member in a Request is discouraged.
                if(id->is_null())
                {
                    ec = rpc_error::invalid_null_id;
                    return;
                }

                if( ! id->is_number() &&
                    ! id->is_string())
                {
                    ec = rpc_error::expected_strnum_id;
                    return;
                }
            }
        }
        else
        {
            // id must be present in 1.0
            if(! id.has_value())
            {
                ec = rpc_error::expected_id;
                return;
            }
        }
    }

    // extract method
    {
        auto it = obj.find("method");
        if(it == obj.end())
        {
            ec = rpc_error::missing_method;
            return;
        }
        if(! it->second.is_string())
        {
            ec = rpc_error::expected_string_method;
            return;
        }
        method = std::move(
            it->second.as_string());
    }

    // extract params
    {
        auto it = obj.find("params");
        if(version == 2)
        {
            if(it != obj.end())
            {
                if( ! it->second.is_object() &&
                    ! it->second.is_array())
                {
                    ec = rpc_error::expected_structured_params;
                    return;
                }
                params = std::move(it->second);
            }
        }
        else
        {
            if(it == obj.end())
            {
                ec = rpc_error::missing_params;
                return;
            }
            if(! it->second.is_array())
            {
                ec = rpc_error::expected_array_params;
                return;
            }
            params = std::move(it->second);
        }
    }
}

//------------------------------------------------------------------------------

json::value
make_rpc_error(
    rpc_error ev,
    beast::string_view msg)
{
    json::value jv;
    jv["jsonrpc"] = "2.0";
    auto& err = jv["error"].emplace_object();
    err["code"] = static_cast<int>(ev);
    err["message"] = msg;
    jv["id"] = nullptr;
    return jv;
}

json::value
make_rpc_error(
    rpc_error ev,
    beast::string_view msg,
    rpc_request const& req)
{
    json::value jv;
    jv["jsonrpc"] = "2.0";
    auto& err = jv["error"].emplace_object();
    err["code"] = static_cast<int>(ev);
    err["message"] = msg;
    if(req.id.has_value())
        jv["id"] = *req.id;
    return jv;
}

json::object&
checked_object(json::value& jv)
{
    if(! jv.is_object())
        throw rpc_exception{};
    return jv.as_object();
}

json::array&
checked_array(json::value& jv)
{
    if(! jv.is_array())
        throw rpc_exception{};
    return jv.as_array();
}

json::string&
checked_string(json::value& jv)
{
    if(! jv.is_string())
        throw rpc_exception{};
    return jv.as_string();
}

json::number&
checked_number(json::value& jv)
{
    if(! jv.is_number())
        throw rpc_exception{};
    return jv.as_number();
}

bool&
checked_bool(json::value& jv)
{
    if(! jv.is_bool())
        throw rpc_exception{};
    return jv.as_bool();
}

void
checked_null(json::value& jv)
{
    if(! jv.is_null())
        throw rpc_exception{};
}

json::value&
checked_value(
    json::value& jv,
    beast::string_view key)
{
    auto& obj =
        checked_object(jv);
    auto it = obj.find(key);
    if(it == obj.end())
        throw rpc_exception{};
    return it->second;
}

json::object&
checked_object(
    json::value& jv,
    beast::string_view key)
{
    return checked_object(
        checked_value(jv, key));
}

json::array&
checked_array(
    json::value& jv,
    beast::string_view key)
{
    return checked_array(
        checked_value(jv, key));
}

json::string&
checked_string(
    json::value& jv,
    beast::string_view key)
{
    return checked_string(
        checked_value(jv, key));
}

json::number&
checked_number(
    json::value& jv,
    beast::string_view key)
{
    return checked_number(
        checked_value(jv, key));
}

bool&
checked_bool(
    json::value& jv,
    beast::string_view key)
{
    return checked_bool(
        checked_value(jv, key));
}

void
checked_null(
    json::value& jv,
    beast::string_view key)
{
    checked_null(checked_value(jv, key));
}
