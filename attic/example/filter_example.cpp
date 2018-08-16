#include "afio_pch.hpp"

//[filter_example
using namespace boost::afio;

// This function will be called for every file opened
static void open_file_filter(detail::OpType, future<> &op) noexcept
{
    std::cout << "File handle " << op->native_handle() << " opened!" << std::endl;
}

// This function will be called for every read and write performed
static void readwrite_filter(detail::OpType optype, handle *h,
    const detail::io_req_impl<true> &req, boost::afio::off_t offset, size_t buffer_idx,
    size_t buffers, const boost::system::error_code &ec, size_t bytes_transferred)
{
    std::cout << "File handle " << h->native_handle() << (detail::OpType::read==optype ?
        " read " : " wrote ") << bytes_transferred << " bytes at offset " << offset << std::endl;
}

int main(void)
{
    dispatcher_ptr dispatcher=
        make_dispatcher().get();
        
    // Install filters BEFORE scheduling any ops as the filter APIs are NOT
    // threadsafe. This filters all file opens.
    dispatcher->post_op_filter({ std::make_pair(detail::OpType::file /* just file opens */,
        std::function<dispatcher::filter_t>(open_file_filter)) });

    // This filters all reads and writes
    dispatcher->post_readwrite_filter({ std::make_pair(detail::OpType::Unknown /* all */,
        std::function<dispatcher::filter_readwrite_t>(readwrite_filter)) });
    return 0;
}
//]
