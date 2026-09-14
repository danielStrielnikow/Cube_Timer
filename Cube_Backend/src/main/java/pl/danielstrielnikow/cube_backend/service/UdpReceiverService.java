package pl.danielstrielnikow.cube_backend.service;

import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;
import pl.danielstrielnikow.cube_backend.model.dto.CubeEventDTO;
import tools.jackson.databind.ObjectMapper;

import java.net.DatagramPacket;
import java.net.DatagramSocket;

@Service
@Slf4j
public class UdpReceiverService {

    private final CubeService cubeService;
    private final ObjectMapper objectMapper;

    public UdpReceiverService(CubeService cubeService, ObjectMapper objectMapper) {
        this.cubeService = cubeService;
        this.objectMapper = objectMapper;
        startListening();
    }

    private void startListening() {
        Thread thread = new Thread(() -> {
            try (DatagramSocket socket = new DatagramSocket(5000)) {
                byte[] buffer = new byte[1024];
                log.info("Serwer UDP nasłuchuje na porcie 5000...");

                while (true) {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    socket.receive(packet);

                    String json = new String(packet.getData(), 0, packet.getLength());
                    log.info("Odebrano sygnał: {}", json);

                    try {
                        CubeEventDTO event = objectMapper.readValue(json, CubeEventDTO.class);
                        cubeService.processEvent(event);
                    } catch (Exception e) {
                        log.error("Błąd przetwarzania JSON: {}", e.getMessage());
                    }
                }
            } catch (Exception e) {
                log.error("Błąd gniazda UDP: {}", e.getMessage());
            }
        });
        thread.setDaemon(true);
        thread.start();
    }
}