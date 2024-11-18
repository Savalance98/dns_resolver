#include <boost/asio.hpp>
#include <boost/beast.hpp>
// #include <boost/property_tree/ptree.hpp>
// #include <boost/property_tree/json_parser.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <vector>
#include <pqxx/pqxx>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

bool does_ip_hostname_exist(pqxx::connection& conn, const std::string& perem, pqxx::work& txn, const std::string& key) {
    pqxx::result result;
    // // Запрос для проверки существования IP
    if (key == "ip"){
        result = txn.exec_params("SELECT 1 FROM ip_addresses WHERE ip = $1;", perem);
    }else if (key == "hostname"){
        result = txn.exec_params("SELECT 1 FROM ip_addresses WHERE hostname = $1;", perem);
    }
    // std::cout << (key == "ip") << " " << result.empty() << std::endl;
    // pqxx::result result = txn.exec_params("SELECT 1 FROM ip_addresses WHERE ip = $1;", perem);

    if (result.empty()) {
        std::cout << "Запись с таким IP не найдена." << std::endl;
        return false;
    } else {
        std::cout << "Запись с таким IP найдена." << std::endl;
        return true;
    }
}

void insert_ip_hostname_if_not_exists(pqxx::connection& conn, const std::string& ip, const std::string& hostname,  pqxx::work& txn) {
    txn.exec_params("INSERT INTO ip_addresses (ip, hostname) VALUES ($1, $2);", ip, hostname);
    txn.commit();
    std::cout << "Запись с IP " << ip << " и hostname " << hostname << " добавлена." << std::endl;
}



std::optional<std::string> get_by_ip_host(pqxx::connection& conn, const std::string& perem,  pqxx::work& txn,boost::json::object& dict, const std::string& key) {

    // Выполнение запроса для получения hostname по IP
    if (key == "ip"){
        pqxx::result result = txn.exec_params("SELECT hostname FROM ip_addresses WHERE ip = $1;", perem);
        // Проверка, есть ли результат
        if (result.empty()) {
            std::cout << "Запись с таким IP не найдена." << std::endl;
            return std::nullopt; // возвращаем пустое значение, если IP не найден
        } else {
            std::string hostname = result[0][0].as<std::string>();
            std::cout << "Hostname для IP " << perem << ": " << hostname << std::endl;
            dict[perem] = hostname;
            return hostname;
        }
    } else if (key == "hostname") {
        pqxx::result result = txn.exec_params("SELECT ip FROM ip_addresses WHERE hostname = $1;", perem);
        // Проверка, есть ли результат
        if (result.empty()) {
            std::cout << "Запись с таким HOST не найдена." << std::endl;
            return std::nullopt; // возвращаем пустое значение, если IP не найден
        } else {
            boost::json::array json_array_res;
            for (const auto pt : result) {
                json_array_res.push_back(boost::json::value(pt[0].as<std::string>())); 
            }
            // std::string ip = result[0][0].as<std::string>();
            // std::cout << "Hostname " << perem << ": " << ip << std::endl;
            dict[perem] = json_array_res;
            return perem;
        }
    }
}


void handleRequest(http::request<http::string_body>& request, tcp::socket& socket) {
        // Подключение к базе данных
    std::cout << "Пытаюсь подключиться к базе данных..." << std::endl;
    pqxx::connection conn("dbname=postgres user=daniilsavin host=/tmp port=5432");
    if (conn.is_open()) {
        std::cout << "Успешное подключение к базе данных: " << conn.dbname() << std::endl;
    } else {
        std::cerr << "Не удалось подключиться к базе данных." << std::endl;
    }

    pqxx::work txn(conn); //создаю транзакцию
    boost::json::object dict; // словарь для вывода данных
    // Создать табл, если раньше не было
    
    // txn.exec("CREATE TABLE IF NOT EXISTS ip_addresses (id SERIAL PRIMARY KEY, ip INET NOT NULL, hostname TEXT NOT NULL);");
    // txn.commit();

    // Вставка IP-адреса
    // std::string ip = "123.2.2.1";
    // pqxx::work txn2(conn);
    // txn2.exec_params("INSERT INTO ip_addresses (ip) VALUES ($1);", ip);
    // txn2.commit();

    std::cout << "IP-адрес успешно добавлен в базу данных." << std::endl;
    //boost::property_tree::ptree pt;
    boost::json::value jv = boost::json::parse(request.body());
    std::cout << request.body() << std::endl;
    if (jv.is_object()) {
        // Получение значения по ключу "ip"
        boost::json::object obj = jv.as_object();
        std::cout << jv <<"ПАПАВПАПВАЫПМУ"<< std::endl;
        if (obj.contains("ip") && obj["ip"].is_array()) {
            // Преобразование JSON-массива в std::vector<std::string>
            std::vector<std::string> ip_list = boost::json::value_to<std::vector<std::string>>(obj["ip"]);
            // Обработка элементов вектора
            for (const auto& ip : ip_list) {
                if (does_ip_hostname_exist(conn, ip, txn, "ip")) {
                // Если ip существует, получаем hostname
                    auto hostname = get_by_ip_host(conn, ip, txn,dict, "ip");
                    if (hostname) {
                    std::cout << "HOSTNAME: " << *hostname << std::endl;
                }
                } else {
                    std::string hostname = "ya.ru";
                    insert_ip_hostname_if_not_exists(conn, ip, hostname, txn);
                    std::cout << "IP-адрес не найден." << std::endl;
                }
                    std::cout << ip << std::endl;
                    
                }
        } else if (obj.contains("hostname") && obj["hostname"].is_array()){
            std::cout << "HOSTNAME!!!!" << std::endl;
        std::vector<std::string> hostname_list = boost::json::value_to<std::vector<std::string>>(obj["hostname"]);
        for (const auto& host : hostname_list) {
                if (does_ip_hostname_exist(conn, host, txn, "hostname")) {
                // Если host существует, получаем ip
                    auto ip = get_by_ip_host(conn, host, txn,dict, "hostname");
                    if (ip) {
                    std::cout << "IP: " << *ip << std::endl;
                }
                } else {
                    std::string ip = "125.0.90.1";
                    insert_ip_hostname_if_not_exists(conn, ip, host, txn);
                    std::cout << "IP-адрес не найден." << std::endl;
                }
                    std::cout << host << std::endl;
                }
    } else {
        std::cerr << "Ошибка: JSON не является объектом." << std::endl;
    }
    }

    
        // Шаг 2: Преобразование в JSON-объект
    std::cout << dict << "СЛОВАРЬ" << std::endl;
    boost::json::value json_value = dict;

    // Шаг 3: Преобразование JSON-объекта в строку
    std::string json_str = boost::json::serialize(json_value);

    // Подготовка ответа
    http::response<http::string_body> response;
    // response.version(request.version());
    response.result(http::status::ok);
    response.set(http::field::server, "My HTTP Server");
    response.set(http::field::content_type, "application/json");
    response.body() = json_str;
    response.prepare_payload();

    // Отправить ответ клиенту
    boost::beast::http::write(socket, response);
}

void runServer() {
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, {tcp::v4(), 8080});

    while (true) {
        tcp::socket socket(io_context);
        acceptor.accept(socket);

        // Read the HTTP request
        boost::beast::flat_buffer buffer;
        http::request<http::string_body> request;
        boost::beast::http::read(socket, buffer, request);

        // Handle the request
        handleRequest(request, socket);

        // Close the socket
        socket.shutdown(tcp::socket::shutdown_send);
    }
}

int main() {
    try {
        runServer();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}