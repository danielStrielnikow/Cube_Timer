package pl.danielstrielnikow.cube_backend.service;

import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import pl.danielstrielnikow.cube_backend.model.Cube;
import pl.danielstrielnikow.cube_backend.model.WorkSession;
import pl.danielstrielnikow.cube_backend.model.dto.CubeEventDTO;
import pl.danielstrielnikow.cube_backend.model.dto.CubeStateMessage;
import pl.danielstrielnikow.cube_backend.repository.CubeRepository;
import pl.danielstrielnikow.cube_backend.repository.WorkSessionRepository;

import java.time.LocalDateTime;

@Service
@RequiredArgsConstructor
public class CubeService {

    private final WorkSessionRepository sessionRepository;
    private final CubeRepository cubeRepository;
    private final WebSocketService webSocketService;

    @Transactional
    public void processEvent(CubeEventDTO event) {
        // 1. Znajdź kostkę w bazie
        Cube cube = cubeRepository.findBySerialNumber(event.getCubeId())
                .orElseThrow(() -> new RuntimeException("Cube not found: " + event.getCubeId()));

        // 2. Znajdź i zamknij poprzednią aktywną sesję
        sessionRepository.findFirstByCubeAndEndTimeIsNullOrderByStartTimeDesc(cube)
                .ifPresent(session -> {
                    if (session.getSideNumber().equals(event.getSide())) {
                        return; // Jeśli bok się nie zmienił, nic nie rób
                    }
                    session.setEndTime(LocalDateTime.now());
                    sessionRepository.save(session);

                    // NOWE: Wyślij WebSocket - sesja zakończona
                    webSocketService.sendCubeUpdate(CubeStateMessage.builder()
                            .type("session_ended")
                            .cubeId(cube.getSerialNumber())
                            .cubeName(cube.getName())
                            .currentSide(session.getSideNumber())
                            .battery(event.getBattery())
                            .startTime(session.getStartTime())
                            .endTime(session.getEndTime())
                            .durationSeconds(session.getDurationSeconds())
                            .timestamp(LocalDateTime.now())
                            .build());
                });

        // 3. Jeśli bok jest > 0 (0 to uśpienie), otwórz nową sesję
        if (event.getSide() > 0) {
            WorkSession newSession = new WorkSession();
            newSession.setCube(cube);
            newSession.setSideNumber(event.getSide());
            newSession.setStartTime(LocalDateTime.now());
            sessionRepository.save(newSession);

            // NOWE: Wyślij WebSocket - nowa sesja
            webSocketService.sendCubeUpdate(CubeStateMessage.builder()
                    .type("session_started")
                    .cubeId(cube.getSerialNumber())
                    .cubeName(cube.getName())
                    .currentSide(event.getSide())
                    .battery(event.getBattery())
                    .startTime(newSession.getStartTime())
                    .timestamp(LocalDateTime.now())
                    .build());
        } else {
            // Side = 0 (uśpienie)
            webSocketService.sendCubeUpdate(CubeStateMessage.builder()
                    .type("cube_status")
                    .cubeId(cube.getSerialNumber())
                    .cubeName(cube.getName())
                    .currentSide(null)
                    .battery(event.getBattery())
                    .timestamp(LocalDateTime.now())
                    .build());
        }
    }
}
