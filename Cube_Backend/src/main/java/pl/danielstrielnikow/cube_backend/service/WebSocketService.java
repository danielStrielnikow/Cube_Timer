package pl.danielstrielnikow.cube_backend.service;

import lombok.RequiredArgsConstructor;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;
import pl.danielstrielnikow.cube_backend.model.dto.CubeStateMessage;

@Service
@RequiredArgsConstructor
public class WebSocketService {

    private final SimpMessagingTemplate messagingTemplate;

    public void sendCubeUpdate(CubeStateMessage message) {
        messagingTemplate.convertAndSend("/topic/cube-updates", message);
    }
}
