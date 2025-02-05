// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "exec/schema_scanner/schema_schemata_scanner.h"

#include "exec/schema_scanner/schema_helper.h"
#include "runtime/primitive_type.h"
#include "vec/common/string_ref.h"

namespace doris {

SchemaScanner::ColumnDesc SchemaSchemataScanner::_s_columns[] = {
        //   name,       type,          size
        {"CATALOG_NAME", TYPE_VARCHAR, sizeof(StringRef), true},
        {"SCHEMA_NAME", TYPE_VARCHAR, sizeof(StringRef), false},
        {"DEFAULT_CHARACTER_SET_NAME", TYPE_VARCHAR, sizeof(StringRef), false},
        {"DEFAULT_COLLATION_NAME", TYPE_VARCHAR, sizeof(StringRef), false},
        {"SQL_PATH", TYPE_VARCHAR, sizeof(StringRef), true},
};

SchemaSchemataScanner::SchemaSchemataScanner()
        : SchemaScanner(_s_columns, sizeof(_s_columns) / sizeof(SchemaScanner::ColumnDesc)),
          _db_index(0) {}

SchemaSchemataScanner::~SchemaSchemataScanner() = default;

Status SchemaSchemataScanner::start(RuntimeState* state) {
    if (!_is_init) {
        return Status::InternalError("used before initial.");
    }
    TGetDbsParams db_params;
    if (nullptr != _param->wild) {
        db_params.__set_pattern(*(_param->wild));
    }
    if (nullptr != _param->catalog) {
        db_params.__set_catalog(*(_param->catalog));
    }
    if (nullptr != _param->current_user_ident) {
        db_params.__set_current_user_ident(*(_param->current_user_ident));
    } else {
        if (nullptr != _param->user) {
            db_params.__set_user(*(_param->user));
        }
        if (nullptr != _param->user_ip) {
            db_params.__set_user_ip(*(_param->user_ip));
        }
    }

    if (nullptr != _param->ip && 0 != _param->port) {
        RETURN_IF_ERROR(
                SchemaHelper::get_db_names(*(_param->ip), _param->port, db_params, &_db_result));
    } else {
        return Status::InternalError("IP or port doesn't exists");
    }

    return Status::OK();
}

Status SchemaSchemataScanner::fill_one_row(Tuple* tuple, MemPool* pool) {
    // set all bit to not null
    memset((void*)tuple, 0, _tuple_desc->num_null_bytes());

    // catalog
    {
        if (!_db_result.__isset.catalogs) {
            tuple->set_null(_tuple_desc->slots()[0]->null_indicator_offset());
        } else {
            void* slot = tuple->get_slot(_tuple_desc->slots()[0]->tuple_offset());
            StringRef* str_slot = reinterpret_cast<StringRef*>(slot);
            std::string catalog_name = _db_result.catalogs[_db_index];
            str_slot->data = (char*)pool->allocate(catalog_name.size());
            str_slot->size = catalog_name.size();
            memcpy(const_cast<char*>(str_slot->data), catalog_name.c_str(), str_slot->size);
        }
    }
    // schema
    {
        void* slot = tuple->get_slot(_tuple_desc->slots()[1]->tuple_offset());
        StringRef* str_slot = reinterpret_cast<StringRef*>(slot);
        std::string db_name = SchemaHelper::extract_db_name(_db_result.dbs[_db_index]);
        str_slot->data = (char*)pool->allocate(db_name.size());
        str_slot->size = db_name.size();
        memcpy(const_cast<char*>(str_slot->data), db_name.c_str(), str_slot->size);
    }
    // DEFAULT_CHARACTER_SET_NAME
    {
        void* slot = tuple->get_slot(_tuple_desc->slots()[2]->tuple_offset());
        StringRef* str_slot = reinterpret_cast<StringRef*>(slot);
        str_slot->size = strlen("utf8") + 1;
        str_slot->data = (char*)pool->allocate(str_slot->size);
        if (nullptr == str_slot->data) {
            return Status::InternalError("Allocate memory failed.");
        }
        memcpy(const_cast<char*>(str_slot->data), "utf8", str_slot->size);
    }
    // DEFAULT_COLLATION_NAME
    {
        void* slot = tuple->get_slot(_tuple_desc->slots()[3]->tuple_offset());
        StringRef* str_slot = reinterpret_cast<StringRef*>(slot);
        str_slot->size = strlen("utf8_general_ci") + 1;
        str_slot->data = (char*)pool->allocate(str_slot->size);
        if (nullptr == str_slot->data) {
            return Status::InternalError("Allocate memory failed.");
        }
        memcpy(const_cast<char*>(str_slot->data), "utf8_general_ci", str_slot->size);
    }
    // SQL_PATH
    { tuple->set_null(_tuple_desc->slots()[4]->null_indicator_offset()); }
    _db_index++;
    return Status::OK();
}

Status SchemaSchemataScanner::get_next_row(Tuple* tuple, MemPool* pool, bool* eos) {
    if (!_is_init) {
        return Status::InternalError("Used before Initialized.");
    }
    if (nullptr == tuple || nullptr == pool || nullptr == eos) {
        return Status::InternalError("input pointer is nullptr.");
    }
    if (_db_index >= _db_result.dbs.size()) {
        *eos = true;
        return Status::OK();
    }
    *eos = false;
    return fill_one_row(tuple, pool);
}

} // namespace doris
