
#ifndef __PAGER_H
#define __PAGER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>
#include "frontend.hpp"
#define PAGE_SIZE 4096

enum 
{
    EXEC_SUCCESS,
    ERROR_TABLE_NOT_FOUND,
    ERROR_ATTRIBUTE_MISMATCH,
    ERROR_ATTRIBUTE_UNDERFLOW, 
    ERROR_ATTRIBUTE_OVERFLOW, 
} EXEC_CODE;




std::unordered_map <NODE_SET , char> schema_type_conversion = {
    {NODE_INT    , 'i'},
    {NODE_FLOAT  , 'f'},
    {NODE_STRING , 's'},
};

std::unordered_map <char , uint32_t> size_conversion = {
    {'i' , sizeof(int)},
    {'f' , sizeof(float)},
    {'s' , 10}, // change this behaviour in the parser
};


uint32_t get_attribute_count (std::string &table_schema)
{
    int semi_colon_count = 0;
    for (char current : table_schema)
        if (current == ';')
            semi_colon_count++;
    return semi_colon_count + 1;
}




struct Page
{

};

struct HeapFile_Metadata
{
    uint32_t total_offset;
    uint32_t page_count;
    uint32_t record_size;
    uint32_t record_count;
    uint32_t write_page_id;
    uint32_t attribute_count;
    uint32_t schema_offset;
    std::string schema;
    HeapFile_Metadata(){};
    HeapFile_Metadata(std::string _schema , uint32_t _record_size)
    {
        schema = _schema;
        schema_offset = _schema.length();
        record_size = _record_size;
        record_count = 0;
        page_count = 0;
        write_page_id = 0;
        attribute_count = get_attribute_count(_schema);
        total_offset = sizeof(uint32_t) * 7 + schema_offset;  
    }
    
};





class Pager
{
    private :
    bool heap_file_exists(std::string* table_name)
    {
        std::ifstream test_stream;
        std::string heap_name = *table_name + ".dat";
        test_stream.open(heap_name);
        bool status = test_stream.good();
        test_stream.close();
        return status;
    } 

    std::string construct_schema(std::vector<AST_NODE *>& table_attribute)
    {
        std::string schema = "";
        for (AST_NODE * current_attribute : table_attribute)
            schema.append(schema_type_conversion[current_attribute->NODE_TYPE] + *current_attribute->PAYLOAD + ';');
        schema.pop_back(); // removing the trailing ; to save a byte
        return schema;
    }

    uint32_t get_size(std::string& table_schema)
    {
        uint32_t record_size = 0;
        int iterator_counter = 0;
        record_size += size_conversion[table_schema[0]];
        while (iterator_counter < table_schema.length())
        {
            if (table_schema[iterator_counter] == ';')
                record_size += size_conversion[table_schema[iterator_counter + 1]];
            iterator_counter++; 
        }
        return record_size;
    }
    public :
    Pager()
    {

    }

    std::vector<std::string> split_schema(std::string & table_schema)
    {
        std::vector<std::string> schema_chunks;
        std::string construct = "";
        for (char current : table_schema)
        {
            if (current == ';')
            {
                schema_chunks.push_back(construct);
                construct = "";
            }
            else    
                construct += current;
        }
        schema_chunks.push_back(construct);
        return schema_chunks;
    }

    void * serialize (std::vector<AST_NODE *>& record , uint32_t record_size , std::vector<std::string>& schema_chunks)
    {
        if (record.size() != schema_chunks.size())
        {
            // THIS ERROR IS SOMEHOW CORRUPTING THE HEAP HEADERS 
            // FIX THIS
            std::cout << "Attribute Size Error ! could not insert the data : " << std::endl;
            exit(1);
        }
        void * serialized_block = malloc(record_size);
        uint32_t offset_accumalator = 0;
        for (int itr = 0 ; itr < record.size() ; itr++)
        {
            AST_NODE * current_attribute = record[itr];
            std::string current_schema_chunk = schema_chunks[itr];
            if (schema_type_conversion[current_attribute->NODE_TYPE] != current_schema_chunk[0])
            {
                // THIS ERROR IS SOMEHOW CORRUPTING THE HEAP HEADERS 
                // FIX THIS
                std::cout << "Attribute mismatch error ! " << std::endl; 
                exit(1);
            }
            switch(current_attribute->NODE_TYPE)
            {
                case NODE_INT :
                {
                    int type_cast_buffer = std::stoi(*current_attribute->PAYLOAD);
                    memcpy(static_cast <char *> (serialized_block) + offset_accumalator , &type_cast_buffer , sizeof(int));
                    break;
                }
                case NODE_FLOAT :
                {
                    float type_cast_buffer = std::stof(*current_attribute->PAYLOAD);
                    memcpy(static_cast <char *> (serialized_block) + offset_accumalator , &type_cast_buffer , sizeof(float));
                    break;
                    
                }
                case NODE_STRING :
                {
                    std::string padded_string = *current_attribute->PAYLOAD;
                    uint32_t required_padding_size = size_conversion[current_schema_chunk[0]] - padded_string.length(); 
                    for (int i = 0 ; i < required_padding_size ; i++)
                        padded_string.push_back('\0');
                    memcpy(static_cast<char *> (serialized_block) + offset_accumalator , padded_string.data() , size_conversion[current_schema_chunk[0]]);
                    break;
                } 

            }
            offset_accumalator += size_conversion[current_schema_chunk[0]];
        }
        return serialized_block;
    }

    void write_heapfile_metadata(std::ofstream& heap_write_stream , HeapFile_Metadata & required_headers)
    {
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.total_offset) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.page_count) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.record_count) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.record_size) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.write_page_id) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.attribute_count) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.schema_offset) , sizeof(uint32_t));

        // attaching the dynamic schema to the header
        heap_write_stream.write(required_headers.schema.c_str() , required_headers.schema_offset);
    }
    
    void update_heapfile_metadata(std::fstream& heap_write_stream , HeapFile_Metadata & required_headers)
    {
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.total_offset) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.page_count) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.record_count) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.record_size) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.write_page_id) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.attribute_count) , sizeof(uint32_t));
        heap_write_stream.write(reinterpret_cast<char *> (&required_headers.schema_offset) , sizeof(uint32_t));

        // attaching the dynamic schema to the header
        heap_write_stream.write(required_headers.schema.c_str() , required_headers.schema_offset);
    }

    
    bool add_to_heap(AST_NODE *& action_node)
    {
        if (!heap_file_exists(action_node->PAYLOAD))
        {
            std::cout << "Error , the given table does not exist , cannot insert data to it ! " << std::endl;
            return false;
        }
        std::ifstream heap_reader_stream(*action_node->PAYLOAD + ".dat" , std::ios::binary);
        HeapFile_Metadata current_heap_metadata =  deserialize_heapfile_metadata(heap_reader_stream);
        heap_reader_stream.close();
        
        std::vector<std::string> schema_chunks = split_schema(current_heap_metadata.schema);
        
        std::fstream heap_write_stream(*action_node->PAYLOAD + ".dat" , std::ios::in | std::ios::out | std::ios::binary );
        heap_write_stream.seekp(current_heap_metadata.total_offset +  current_heap_metadata.record_size * current_heap_metadata.record_count);

        for (std::vector<AST_NODE *> record : action_node->MULTI_DATA)
        {
            void * serialized_block = serialize(record , current_heap_metadata.record_size , schema_chunks);
            heap_write_stream.write(static_cast<char *> (serialized_block) , current_heap_metadata.record_size);    
            current_heap_metadata.record_count++;
        }

        heap_write_stream.seekp(0 , std::ios::beg); // resetting the pointer to the start of the file 
        // to write back the updated heap file metadata
        update_heapfile_metadata(heap_write_stream , current_heap_metadata);
        std::cout << "closing the stream : " << std::endl;
        heap_write_stream.close();
        return true;
    }
    bool create_new_heap(AST_NODE *& action_node)
    {
        // working on the same database , change the behaviour for multi db
        std::cout << "this is the start of the create new heap function : " << std::endl;
        if (heap_file_exists(action_node->PAYLOAD))
            return false;
        std::cout << "creating the new heap : " << std::endl;
        std::string table_schema = construct_schema(action_node->CHILDREN);
        uint32_t record_size = get_size(table_schema);
        HeapFile_Metadata current_metadata (table_schema , record_size);
        
        std::ofstream heap_write_stream(*action_node->PAYLOAD + ".dat" , std::ios::binary);
        // writing the heap file header to the file
        write_heapfile_metadata(heap_write_stream , current_metadata);
        heap_write_stream.close();
        return true;
    }

    HeapFile_Metadata deserialize_heapfile_metadata (std::ifstream& heap_read_stream)
    {
        HeapFile_Metadata heapfile_headers;
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.total_offset) , sizeof(uint32_t));
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.page_count) , sizeof(uint32_t));
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.record_count) , sizeof(uint32_t));
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.record_size) , sizeof(uint32_t));
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.write_page_id) , sizeof(uint32_t));
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.attribute_count) , sizeof(uint32_t));
        heap_read_stream.read(reinterpret_cast <char *> (&heapfile_headers.schema_offset) , sizeof(uint32_t));

        char * schema_buffer_pointer = (char *)malloc (heapfile_headers.schema_offset + 1);
        heap_read_stream.read(schema_buffer_pointer ,heapfile_headers.schema_offset);
        schema_buffer_pointer[heapfile_headers.schema_offset] = '\0';

        heapfile_headers.schema.assign(schema_buffer_pointer);                
        free(schema_buffer_pointer);
        return heapfile_headers;
    }
    bool get_heap(AST_NODE *& action_node)
    {

        if (!this->heap_file_exists(&action_node->DATA_LIST[0]))
            return false;
        // std::cout << "[*] Table Name : " << action_node->DATA_LIST[0] << std::endl;
        // std::cout << "[*] Heap File : " << action_node->DATA_LIST[0]+ ".dat" << std::endl;
        std::ifstream heap_read_stream (action_node->DATA_LIST[0] + ".dat" , std::ios::binary);

        HeapFile_Metadata current_heapfile_metadata = deserialize_heapfile_metadata(heap_read_stream);
        
        // std::cout << "[*] Schema Offset : " << current_heapfile_metadata.schema_offset << std::endl;
        // std::cout << "[*] Page Count : " << current_heapfile_metadata.page_count << std::endl;
        // std::cout << "[*] Record Count : " << current_heapfile_metadata.record_count <<  std::endl;
        // std::cout << "[*] Record Size : " << current_heapfile_metadata.record_size <<  std::endl;
        // std::cout << "[*] Write Page ID : " << current_heapfile_metadata.write_page_id <<  std::endl;
        // std::cout << "[*] Total Offset : " << current_heapfile_metadata.total_offset <<  std::endl;
        // std::cout << "[*] Schema : " << current_heapfile_metadata.schema << std::endl;
        // std::cout << "[*] Attribute Count : " << current_heapfile_metadata.attribute_count << std::endl;
        // std::cout << "this is the current offset of the ifstream : " << heap_read_stream.tellg() << std::endl;
        
        std::vector<std::string> schema_chunks = split_schema(current_heapfile_metadata.schema);
        for (std::string attribute : schema_chunks)
            std::cout << attribute.substr(1) << "\t";
        std::cout << std::endl;

        for (int record_counter = 0 ; record_counter < current_heapfile_metadata.record_count ; record_counter++)
            deserialize(heap_read_stream , schema_chunks);
        
        heap_read_stream.close(); 
        return true;
    }

    void deserialize(std::ifstream& heap_read_stream , std::vector<std::string>& schema_chunks)
    {
        for (std::string attribute : schema_chunks)
        {
            switch(attribute[0])
            {
                case 'i':
                {
                    int read_buffer;
                    heap_read_stream.read(reinterpret_cast<char *> (&read_buffer) , sizeof(int));
                    std::cout << read_buffer << "\t";
                    break;
                }
                case 'f':
                {
                    float read_buffer;
                    heap_read_stream.read(reinterpret_cast<char *> (&read_buffer) , sizeof(float));
                    std::cout << read_buffer << "\t";
                    break;
                }
                case 's':
                {
                    uint32_t current_size = size_conversion[attribute[0]];
                    char * read_buffer = (char*) malloc(current_size + 1);
                    heap_read_stream.read(read_buffer , current_size);
                    read_buffer[current_size] = '\0';
                    std::cout << read_buffer << "\t";
                    free(read_buffer);
                    break;                

                }


            }
        }
        std::cout << std::endl;
    }



    bool insert_record(std::string table_name , void * record)
    {
        std::ofstream table_heap_file;
        table_heap_file.open(table_name + ".dat" , std::ios::binary);
        

    }
};

#endif