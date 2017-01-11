#include "pch.h"
#include "msServer.h"


namespace ms {

using namespace Poco::Net;


class RequestHandler : public HTTPRequestHandler
{
public:
    RequestHandler(Server *server);
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) override;

private:
    Server *m_server = nullptr;
};

class RequestHandlerFactory : public HTTPRequestHandlerFactory
{
public:
    RequestHandlerFactory(Server *server);
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest &request) override;

private:
    Server *m_server = nullptr;
};



RequestHandler::RequestHandler(Server *server)
    : m_server(server)
{
}

template<class Body>
void Server::recvDelete(const Body& body)
{
    body(m_tmp_del);
    {
        m_data[m_tmp_xf.obj_path].del = m_tmp_del;
        m_history.push_back({ EventType::Delete, m_tmp_del.obj_path });
        m_tmp_del.clear();
    }
}

template<class Body>
void Server::recvXform(const Body& body)
{
    body(m_tmp_xf);
    {
        m_data[m_tmp_xf.obj_path].xform = m_tmp_xf;
        m_history.push_back({ EventType::Xform, m_tmp_xf.obj_path });
        m_tmp_xf.clear();
    }
}

template<class Body>
void Server::recvMesh(const Body& body)
{
    body(m_tmp_mesh);
    if (!m_tmp_mesh.obj_path.empty()) {
        m_tmp_mesh.refine(m_settings.mesh_flags, m_settings.scale);
        m_data[m_tmp_mesh.obj_path].mesh = m_tmp_mesh;
        m_history.push_back({ EventType::Mesh, m_tmp_mesh.obj_path });
        m_tmp_mesh.clear();
    }
}

void RequestHandler::handleRequest(HTTPServerRequest &request, HTTPServerResponse &response)
{
    if (request.getURI() == "delete") {
        m_server->recvMesh([&request](MeshData& tmp) {
            tmp.deserialize(request.stream());
        });
    }
    if (request.getURI() == "xform") {
        m_server->recvXform([&request](XformData& tmp) {
            tmp.deserialize(request.stream());
        });
    }
    if (request.getURI() == "mesh") {
        m_server->recvMesh([this, &request](MeshData& tmp) {
            tmp.deserialize(request.stream());
        });
    }

    std::string ok = "ok";
    response.setContentType("text/plain");
    response.setContentLength(ok.size());
    std::ostream &ostr = response.send();
    ostr.write(ok.c_str(), ok.size());
}

RequestHandlerFactory::RequestHandlerFactory(Server *server)
    : m_server(server)
{
}

HTTPRequestHandler* RequestHandlerFactory::createRequestHandler(const HTTPServerRequest &request)
{
    return new RequestHandler(m_server);
}



Server::Server(const ServerSettings& settings)
    : m_settings(settings)
{
}

Server::~Server()
{
    stop();
}

bool Server::start()
{
    if (!m_server) {
        m_end_flag = false;

        auto* params = new Poco::Net::HTTPServerParams;
        params->setMaxQueued(m_settings.max_queue);
        params->setMaxThreads(1);
        params->setThreadIdleTime(Poco::Timespan(3, 0));

        try {
            Poco::Net::ServerSocket svs(m_settings.port);
            m_server.reset(new Poco::Net::HTTPServer(new RequestHandlerFactory(this), svs, params));
            m_server->start();
        }
        catch (Poco::IOException &e) {
            //wiDebugPrint(e.what());
            //wiAssert(false);
            return false;
        }
    }

    return true;
}

void Server::stop()
{
    m_server.reset();
}

const ServerSettings& Server::getSettings() const
{
    return m_settings;
}

} // namespace ms
