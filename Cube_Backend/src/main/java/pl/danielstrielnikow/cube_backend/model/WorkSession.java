package pl.danielstrielnikow.cube_backend.model;

import jakarta.persistence.*;
import lombok.Data;

import java.time.Duration;
import java.time.LocalDateTime;
import java.util.UUID;

@Entity
@Table(name = "work_sessions")
@Data
public class WorkSession {
    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne
    private Cube cube;

    private Integer sideNumber;

    @Column(nullable = false)
    private LocalDateTime startTime;

    private LocalDateTime endTime;

    // Pole obliczane automatycznie przed zapisem do bazy
    private Long durationSeconds;

    @PrePersist
    @PreUpdate
    public void calculateDuration() {
        if (startTime != null && endTime != null) {
            this.durationSeconds = Duration.between(startTime, endTime).getSeconds();
        }
    }
}
