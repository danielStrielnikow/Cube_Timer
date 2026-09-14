package pl.danielstrielnikow.cube_backend.model.dto;

import lombok.Builder;
import lombok.Data;

import java.time.LocalDateTime;

@Data
@Builder
public class CubeStateMessage {
    private String type;              // "session_started" | "session_ended" | "cube_status"
    private String cubeId;            // "CUBE-01"
    private String cubeName;          // "Main Cube"
    private Integer currentSide;      // 1-6 (null jeśli uśpione)
    private Double battery;           // 3.85V
    private LocalDateTime startTime;  // tylko dla session_started/ended
    private LocalDateTime endTime;    // tylko dla session_ended
    private Long durationSeconds;     // tylko dla session_ended
    private Long elapsedSeconds;      // dla aktywnej sesji - czas od rozpoczęcia
    private LocalDateTime timestamp;  // timestamp wysłania
}
